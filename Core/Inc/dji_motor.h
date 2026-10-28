/**
 * @file dji_motor.h
 * @brief M3508 与 M2006 共用的电机组数据结构。
 *
 * 一个对象对应一个控制帧和四个槽位：
 *   槽位 0 -> 反馈 ID 基地址 + 0
 *   槽位 1 -> 反馈 ID 基地址 + 1
 *   槽位 2 -> 反馈 ID 基地址 + 2
 *   槽位 3 -> 反馈 ID 基地址 + 3
 *
 * 以 0x200 组为例：
 *   槽位 0 的反馈 ID 为 0x201
 *   槽位 1 的反馈 ID 为 0x202
 *   槽位 2 的反馈 ID 为 0x203
 *   槽位 3 的反馈 ID 为 0x204
 *
 * 以 0x1FF 组为例：
 *   槽位 0 的反馈 ID 为 0x205
 *   槽位 1 的反馈 ID 为 0x206
 *   槽位 2 的反馈 ID 为 0x207
 *   槽位 3 的反馈 ID 为 0x208
 */

#ifndef DJI_MOTOR_H
#define DJI_MOTOR_H

#include <stdint.h>
#include "fdcan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    /**
     * 原始控制量，由 SetCurrent() 写入，由 Send() 打包发送。
     * 写入范围：
     *   M3508：-16384～+16384
     *   M2006：-10000～+10000
     */
    int16_t command[4];

    /**
     * 输出轴累计角度，单位 rad。
     * 由编码器值和累计圈数换算得到。
     */
    float position[4];

    /**
     * 输出轴角速度，单位 rad/s。
     * 由反馈帧中的转子转速换算得到。
     */
    float velocity[4];

    /**
     * 反馈电流，单位 A。
     * 由反馈帧中的原始电流值换算得到。
     */
    float current[4];

    /**
     * 方向位：
     *   bit0 对应槽位 0
     *   bit1 对应槽位 1
     *   bit2 对应槽位 2
     *   bit3 对应槽位 3
     *
     * 对应位为 1 时，该槽位的控制量和反馈量同时取反。
     */
    uint8_t reversed;

    /**
     * 内部使用：最近一帧编码器值，范围 0～8191。
     * 初值为 0xFFFF，首帧到达后写入实际编码器值。
     */
    uint16_t encoder[4];

    /**
     * 内部使用：编码器累计圈数。
     * 正转跨过 8191→0 时加一，反转跨过 0→8191 时减一。
     */
    int32_t round[4];
} DjiMotor;



#ifdef __cplusplus
}
#endif

#endif /* DJI_MOTOR_H */