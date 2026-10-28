/**
 * @file motor_2006.c
 * @brief M2006 + C610 电流控制实现。
 *
 * 换算关系：
 *   电流(A)   = 原始电流 × (10 / 10000) × 方向
 *   转速(rad/s) = 转子 rpm × (2π / 60) / 减速比 × 方向
 *   角度(rad) = ((圈数 × 8192) + 编码器) × (2π / 8192) / 减速比 × 方向
 */

#include "motor_2006.h"

#include "can.h"

#include <stddef.h>

/* C610 反馈电流换算系数：10 A 对应 10000。 */
#define MOTOR2006_CURRENT_TO_AMP (10.0f / 10000.0f)

/* M2006 减速比。 */
#define MOTOR2006_GEAR_RATIO 36.0f

/* 圆周常数。 */
#define MOTOR2006_TWO_PI 6.283185307179586f

/* 编码器一圈计数。 */
#define MOTOR2006_ENCODER_MAX 8192

/* 编码器半圈计数，用于跨圈判断。 */
#define MOTOR2006_ENCODER_HALF 4096

Motor2006 g_m2006;

void Motor2006_Init(Motor2006 *motor, uint8_t reversed)
{
    if (motor == NULL)
    {
        return;
    }

    for (uint8_t slot = 0U; slot < 4U; ++slot)
    {
        /* 控制量与反馈量清零。 */
        motor->command[slot] = 0;
        motor->position[slot] = 0.0f;
        motor->velocity[slot] = 0.0f;
        motor->current[slot] = 0.0f;

        /* 编码器初值使用 0xFFFF，首帧到达后写入实际值。 */
        motor->encoder[slot] = 0xFFFFU;

        /* 多圈计数清零。 */
        motor->round[slot] = 0;
    }

    motor->reversed = reversed;
}

void Motor2006_SetCurrent(Motor2006 *motor, uint8_t slot, int16_t current)
{
    if ((motor == NULL) || (slot >= 4U))
    {
        return;
    }

    /* 限制到 C610 原始电流范围。 */
    if (current > MOTOR2006_CURRENT_MAX)
    {
        current = MOTOR2006_CURRENT_MAX;
    }
    else if (current < -MOTOR2006_CURRENT_MAX)
    {
        current = -MOTOR2006_CURRENT_MAX;
    }

    /* 对应槽位的方向位为 1 时，控制量取反。 */
    if ((motor->reversed & (1U << slot)) != 0U)
    {
        current = (int16_t)-current;
    }

    motor->command[slot] = current;
}

void Motor2006_Update(Motor2006 *motor,
                      uint32_t id,
                      const uint8_t *data,
                      uint8_t size)
{
    uint8_t slot;
    uint16_t encoder;
    int16_t speed_rpm;
    int16_t current_raw;
    int32_t difference;
    float direction;

    /* 反馈帧固定为 8 字节。 */
    if ((motor == NULL) || (data == NULL) || (size != 8U))
    {
        return;
    }

    /* 本对象处理 0x205～0x208。 */
    if ((id < MOTOR2006_RX_ID) || (id >= (MOTOR2006_RX_ID + 4U)))
    {
        return;
    }

    /* 反馈 ID 与槽位的对应关系：0x205 -> 0，0x208 -> 3。 */
    slot = (uint8_t)(id - MOTOR2006_RX_ID);

    /* 编码器按 16 位值读取，转速和电流按 int16 大端读取。 */
    encoder = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    speed_rpm = (int16_t)(((uint16_t)data[2] << 8U) | data[3]);
    current_raw = (int16_t)(((uint16_t)data[4] << 8U) | data[5]);

    /* data[6] 为温度，data[7] 为保留字节；本驱动使用编码器、转速和电流。 */

    /*
     * 多圈计数：
     *   编码器从接近 8191 的值跳到接近 0 的值时，判定为正转跨圈，
     *   round 加一；
     *   编码器从接近 0 的值跳到接近 8191 的值时，判定为反转跨圈，
     *   round 减一。
     *
     * 首帧到达时建立基准，round 保持为 0。
     */
    if (motor->encoder[slot] != 0xFFFFU)
    {
        difference = (int32_t)encoder - (int32_t)motor->encoder[slot];

        if (difference < -MOTOR2006_ENCODER_HALF)
        {
            motor->round[slot]++;
        }
        else if (difference > MOTOR2006_ENCODER_HALF)
        {
            motor->round[slot]--;
        }
    }
    else
    {
        motor->round[slot] = 0;
    }

    motor->encoder[slot] = encoder;

    /* 方向位为 1 时取反。 */
    direction = ((motor->reversed & (1U << slot)) != 0U) ? -1.0f : 1.0f;

    /*
     * 输出轴累计角度：
     *   (round × 8192 + encoder) × 2π / 8192 / 减速比。
     */
    motor->position[slot] = direction *
        ((float)motor->round[slot] * (float)MOTOR2006_ENCODER_MAX +
         (float)encoder) *
        (MOTOR2006_TWO_PI / (float)MOTOR2006_ENCODER_MAX) /
        MOTOR2006_GEAR_RATIO;

    /*
     * 输出轴角速度：
     *   转子 rpm × 2π / 60 / 减速比。
     */
    motor->velocity[slot] = direction *
        (float)speed_rpm * (MOTOR2006_TWO_PI / 60.0f) /
        MOTOR2006_GEAR_RATIO;

    /*
     * 反馈电流：
     *   原始电流 × 10 A / 10000。
     */
    motor->current[slot] =
        direction * (float)current_raw * MOTOR2006_CURRENT_TO_AMP;
}

bool Motor2006_Send(Motor2006 *motor)
{
    uint8_t data[8];

    if (motor == NULL)
    {
        return false;
    }

    /*
     * 按大端 int16 打包四个槽位的控制量：
     *   data[0..1] = 槽位 0
     *   data[2..3] = 槽位 1
     *   data[4..5] = 槽位 2
     *   data[6..7] = 槽位 3
     */
    for (uint8_t slot = 0U; slot < 4U; ++slot)
    {
        uint16_t value = (uint16_t)motor->command[slot];

        data[2U * slot] = (uint8_t)(value >> 8U);
        data[2U * slot + 1U] = (uint8_t)value;
    }

    return Can_Send(MOTOR2006_TX_ID, data, 8U);
}