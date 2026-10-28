/**
 * @file fxi6x.c
 * @brief FXI6X SBUS 遥控接收实现。
 *
 * 接收路径：
 *   1. UART5 接收中断把字节写入环形缓冲区。
 *   2. Fxi6x_Update() 在主循环中按 25 字节组帧。
 *   3. 帧头和帧尾匹配后解析 10 个通道。
 */

#include "fxi6x.h"

#include "usart.h"

#include <string.h>

/* SBUS 帧长度。 */
#define FXI6X_FRAME_SIZE 25U

/* 环形缓冲区大小，可缓存约 12.8 ms 的 100 kbaud 数据。 */
#define FXI6X_RING_SIZE 128U

/* 通道中值。 */
#define FXI6X_CENTER 1024

/* 模拟通道有效范围。 */
#define FXI6X_MIN (-784)
#define FXI6X_MAX 783

/* 开关阈值。 */
#define FXI6X_SWITCH_THRESHOLD 392

/* UART5 单字节接收缓存。 */
static uint8_t s_byte;

/* 环形缓冲区读写位置。 */
static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static uint8_t s_ring[FXI6X_RING_SIZE];

/* 组帧缓存和当前长度。 */
static uint8_t s_frame[FXI6X_FRAME_SIZE];
static uint8_t s_len;

/* 最近一帧遥控数据。 */
static Fxi6x_Data s_data;

/**
 * @brief 从环形缓冲区取出一个字节。
 *
 * @param byte 输出字节。
 *
 * @return true  取到字节。
 * @return false 缓冲区为空。
 */
static bool fxi6x_pop(uint8_t *byte)
{
    if (s_tail == s_head)
    {
        return false;
    }

    *byte = s_ring[s_tail];
    s_tail = (uint16_t)((s_tail + 1U) % FXI6X_RING_SIZE);
    return true;
}

/**
 * @brief 从两个字节中提取一个 11 bit 通道。
 *
 * @param high  高字节。
 * @param low   低字节。
 * @param shift 右移位数。
 *
 * @return 减去中值后的通道值，范围 -1024～1023。
 */
static int16_t fxi6x_channel(uint8_t high, uint8_t low, uint8_t shift)
{
    uint16_t raw = (uint16_t)(((uint16_t)high << 8U) | low);

    return (int16_t)((int32_t)((raw >> shift) & 0x07FFU) - FXI6X_CENTER);
}

/**
 * @brief 将模拟通道值归一化到 -1.0～1.0。
 *
 * @param value 通道值，范围 -784～783。
 *
 * @return 归一化结果。
 */
static float fxi6x_normalize(int16_t value)
{
    if (value >= 0)
    {
        float result = (float)value / (float)FXI6X_MAX;

        return (result > 1.0f) ? 1.0f : result;
    }

    {
        float result = (float)value / (float)(-FXI6X_MIN);

        return (result < -1.0f) ? -1.0f : result;
    }
}

/**
 * @brief 将开关通道值映射为 HIGH、MIDDLE、LOW。
 *
 * @param value 通道值。
 *
 * @return 开关状态。
 */
static uint8_t fxi6x_switch(int16_t value)
{
    if (value < -FXI6X_SWITCH_THRESHOLD)
    {
        return FXI6X_HIGH;
    }

    if (value > FXI6X_SWITCH_THRESHOLD)
    {
        return FXI6X_LOW;
    }

    return FXI6X_MIDDLE;
}

/**
 * @brief 校验模拟通道值落在有效范围。
 *
 * @param value 通道值。
 *
 * @return true 表示值在 -784～783 以内。
 */
static bool fxi6x_valid(int16_t value)
{
    return (value >= FXI6X_MIN) && (value <= FXI6X_MAX);
}

/**
 * @brief 将摇杆、旋钮和开关恢复到中位。
 */
static void fxi6x_clear(void)
{
    s_data.rocker_l_h = 0.0f;
    s_data.rocker_l_v = 0.0f;
    s_data.rocker_r_h = 0.0f;
    s_data.rocker_r_v = 0.0f;

    s_data.knob_l = 0.0f;
    s_data.knob_r = 0.0f;

    s_data.switch_a = FXI6X_HIGH;
    s_data.switch_b = FXI6X_HIGH;
    s_data.switch_c = FXI6X_HIGH;
    s_data.switch_d = FXI6X_HIGH;
}

/**
 * @brief 解析一帧 25 字节 SBUS 数据。
 *
 * @param frame 帧缓冲区。
 *
 * @return true  帧有效且处于正常工作状态。
 * @return false 帧为 failsafe 状态，或模拟通道超出有效范围。
 */
static bool fxi6x_decode(const uint8_t *frame)
{
    int16_t channel[10];

    /* 按 11 bit 小端格式提取前 10 个通道。 */
    channel[0] = fxi6x_channel(frame[2], frame[1], 0U);
    channel[1] = fxi6x_channel(frame[3], frame[2], 3U);

    channel[2] = (int16_t)((int32_t)(
        (((uint32_t)frame[5] << 10U) |
         ((uint32_t)frame[4] << 2U) |
         ((uint32_t)frame[3] >> 6U)) & 0x07FFU) - FXI6X_CENTER);

    channel[3] = fxi6x_channel(frame[6], frame[5], 1U);
    channel[4] = fxi6x_channel(frame[7], frame[6], 4U);

    channel[5] = (int16_t)((int32_t)(
        (((uint32_t)frame[9] << 9U) |
         ((uint32_t)frame[8] << 1U) |
         ((uint32_t)frame[7] >> 7U)) & 0x07FFU) - FXI6X_CENTER);

    channel[6] = fxi6x_channel(frame[10], frame[9], 2U);
    channel[7] = fxi6x_channel(frame[11], frame[10], 5U);
    channel[8] = fxi6x_channel(frame[13], frame[12], 0U);
    channel[9] = fxi6x_channel(frame[14], frame[13], 3U);

    /* 读取接收机标志位。 */
    s_data.frame_lost = (frame[23] & 0x04U) != 0U;
    s_data.failsafe = (frame[23] & 0x08U) != 0U;

    /* 写入归一化后的摇杆和旋钮值。 */
    s_data.rocker_r_h = fxi6x_normalize(channel[0]);
    s_data.rocker_r_v = fxi6x_normalize(channel[1]);
    s_data.rocker_l_v = fxi6x_normalize(channel[2]);
    s_data.rocker_l_h = fxi6x_normalize(channel[3]);
    s_data.knob_l = fxi6x_normalize(channel[4]);
    s_data.knob_r = fxi6x_normalize(channel[5]);

    /* 写入开关状态。 */
    s_data.switch_a = fxi6x_switch(channel[6]);
    s_data.switch_b = fxi6x_switch(channel[7]);
    s_data.switch_c = fxi6x_switch(channel[8]);
    s_data.switch_d = fxi6x_switch(channel[9]);

    /*
     * failsafe 帧或模拟通道超出范围时，
     * 将摇杆、旋钮和开关恢复到中位，并返回 false。
     */
    if (s_data.failsafe ||
        (!fxi6x_valid(channel[0])) ||
        (!fxi6x_valid(channel[1])) ||
        (!fxi6x_valid(channel[2])) ||
        (!fxi6x_valid(channel[3])) ||
        (!fxi6x_valid(channel[4])) ||
        (!fxi6x_valid(channel[5])))
    {
        fxi6x_clear();
        return false;
    }

    return true;
}

bool Fxi6x_Init(void)
{
    /* 清空环形缓冲区和组帧状态。 */
    s_head = 0U;
    s_tail = 0U;
    s_len = 0U;

    /* 清空数据，并将开关置于 HIGH。 */
    memset(&s_data, 0, sizeof(s_data));
    s_data.switch_a = FXI6X_HIGH;
    s_data.switch_b = FXI6X_HIGH;
    s_data.switch_c = FXI6X_HIGH;
    s_data.switch_d = FXI6X_HIGH;

    /* 启动 UART5 单字节中断接收。 */
    return HAL_UART_Receive_IT(&huart5, &s_byte, 1U) == HAL_OK;
}

bool Fxi6x_Update(void)
{
    bool got = false;
    uint8_t byte;

    /* 逐字节组帧。 */
    while (fxi6x_pop(&byte))
    {
        /* 帧首字节为 0x0F；组帧开始前跳过其他字节。 */
        if ((s_len == 0U) && (byte != 0x0FU))
        {
            continue;
        }

        s_frame[s_len] = byte;
        s_len++;

        /* 收集满 25 字节后检查帧头和帧尾。 */
        if (s_len == FXI6X_FRAME_SIZE)
        {
            if ((s_frame[0] == 0x0FU) && (s_frame[24] == 0x00U))
            {
                if (fxi6x_decode(s_frame))
                {
                    got = true;
                }
            }

            /* 每帧处理结束后，从下一个字节重新组帧。 */
            s_len = 0U;
        }
    }

    return got;
}

const Fxi6x_Data *Fxi6x_GetData(void)
{
    return &s_data;
}

/**
 * @brief UART5 接收完成回调。
 *
 * 将收到的字节写入环形缓冲区，并重新挂起单字节接收。
 *
 * @param huart 触发回调的 UART 句柄。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART5)
    {
        return;
    }

    uint16_t next = (uint16_t)((s_head + 1U) % FXI6X_RING_SIZE);

    /* 缓冲区有空间时写入字节；空间占满时保留原有数据。 */
    if (next != s_tail)
    {
        s_ring[s_head] = s_byte;
        s_head = next;
    }

    /* 重新挂起单字节接收。 */
    (void)HAL_UART_Receive_IT(&huart5, &s_byte, 1U);
}