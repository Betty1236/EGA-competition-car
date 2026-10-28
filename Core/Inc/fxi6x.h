/**
 * @file fxi6x.h
 * @brief FXI6X SBUS 遥控接收接口。
 *
 * 硬件：
 *   UART5_RX = PD2
 *   UART5_TX = PC12
 *
 * 通信参数：
 *   100000 baud，9 bit 字长，偶校验，1 停止位。
 *
 * 数据帧：
 *   25 字节，0x0F 帧头，0x00 帧尾，16 个 11 bit 通道。
 */

#ifndef FXI6X_H
#define FXI6X_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 开关状态。 */
#define FXI6X_HIGH 0U
#define FXI6X_MIDDLE 1U
#define FXI6X_LOW 2U

typedef struct
{
    /* 摇杆，范围 -1.0～1.0。 */
    float rocker_l_h;
    float rocker_l_v;
    float rocker_r_h;
    float rocker_r_v;

    /* 旋钮，范围 -1.0～1.0。 */
    float knob_l;
    float knob_r;

    /* 开关，取值为 FXI6X_HIGH、FXI6X_MIDDLE、FXI6X_LOW。 */
    uint8_t switch_a;
    uint8_t switch_b;
    uint8_t switch_c;
    uint8_t switch_d;

    /* 接收机标志位。 */
    bool frame_lost;
    bool failsafe;
} Fxi6x_Data;

/**
 * @brief 初始化 FXI6X 驱动。
 *
 * 清空数据结构和环形缓冲区，并启动 UART5 单字节中断接收。
 *
 * @return true  UART5 接收启动成功。
 * @return false UART5 接收启动返回 HAL_ERROR。
 */
bool Fxi6x_Init(void);

/**
 * @brief 解析环形缓冲区中的 SBUS 数据。
 *
 * 函数在主循环中调用，本次调用解析出有效控制帧时返回 true。
 *
 * @return true  本次调用收到并解析了有效控制帧。
 * @return false 本次调用收到 failsafe 帧、范围外帧，或缓冲区中的字节少于 25 字节。
 */
bool Fxi6x_Update(void);

/**
 * @brief 获取最近一帧遥控数据。
 *
 * @return 指向内部数据结构的常量指针。
 */
const Fxi6x_Data *Fxi6x_GetData(void);

#ifdef __cplusplus
}
#endif

#endif /* FXI6X_H */