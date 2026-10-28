/**
 * @file motor_2006.h
 * @brief M2006 + C610 电流控制接口。
 *
 * 控制帧：0x1FF
 * 反馈帧：0x205～0x208
 *
 * 一个 Motor2006 对象管理四个槽位。
 * 槽位号范围 0～3，对应反馈 ID 0x205～0x208。
 */

#ifndef MOTOR_2006_H
#define MOTOR_2006_H

#include <stdbool.h>
#include <stdint.h>

#include "dji_motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 控制帧 ID，对应槽位 0～3 的反馈 ID 为 0x205～0x208。 */
#define MOTOR2006_TX_ID 0x1FFU
#define MOTOR2006_RX_ID 0x205U

/* C610 原始电流范围。 */
#define MOTOR2006_CURRENT_MAX 10000

/* M2006 对象与公共电机结构使用同一组字段。 */
typedef DjiMotor Motor2006;

extern Motor2006 g_m2006;

/**
 * @brief 初始化 M2006 对象。
 *
 * @param motor    电机对象。
 * @param reversed 方向位掩码，bit0～bit3 对应槽位 0～3。
 *
 * 初始化完成后：
 *   command、position、velocity、current 全部清零；
 *   encoder 写入初值 0xFFFF；
 *   round 清零。
 */
void Motor2006_Init(Motor2006 *motor, uint8_t reversed);

/**
 * @brief 设置指定槽位的原始电流。
 *
 * @param motor   电机对象。
 * @param slot    槽位号，范围 0～3。
 * @param current 原始电流，范围 -10000～+10000。
 *
 * 函数内部完成范围限制和方向处理，结果写入 command[slot]。
 */
void Motor2006_SetCurrent(Motor2006 *motor, uint8_t slot, int16_t current);

/**
 * @brief 解析一帧 M2006 反馈。
 *
 * @param motor 电机对象。
 * @param id    CAN 帧 ID。
 * @param data  数据缓冲区，长度 8 字节。
 * @param size  数据长度。
 *
 * 函数处理 ID 为 0x205～0x208、长度为 8 的帧；
 * 其他帧直接返回。该函数在 CAN 接收回调中调用。
 */
void Motor2006_Update(Motor2006 *motor,
                      uint32_t id,
                      const uint8_t *data,
                      uint8_t size);

/**
 * @brief 发送 0x1FF 控制帧。
 *
 * @param motor 电机对象。
 *
 * @return true  帧已写入 CAN 发送 FIFO。
 * @return false 对象为空，或 CAN 发送返回 HAL_ERROR。
 *
 * 四个槽位的 command 按大端 int16 打包为 8 字节。
 */
bool Motor2006_Send(Motor2006 *motor);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_2006_H */