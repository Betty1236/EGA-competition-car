/**
 * @file servo.h
 * @brief TIM1/TIM2 四路 50 Hz 舵机接口。
 *
 * 通道映射：
 *   通道 0 -> TIM1_CH1 / PE9
 *   通道 1 -> TIM1_CH3 / PE13
 *   通道 2 -> TIM2_CH1 / PA0
 *   通道 3 -> TIM2_CH3 / PA2
 *
 * 电源：
 *   PC15，高电平使能。
 */

#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化四路舵机。
 *
 * 配置 50 Hz 时基，四路脉宽设为 1500 μs，
 * 停止 PWM 输出，并将 PC15 拉低。
 */
void Servo_Init(void);

/**
 * @brief 设置指定通道的脉宽。
 *
 * @param channel 通道号，范围 0～3。
 * @param pulse_us 脉宽，范围 0～20000 μs。
 *
 * 函数只更新比较值，PWM 输出状态由 Servo_On() 和 Servo_Off() 控制。
 */
void Servo_Set(uint8_t channel, uint16_t pulse_us);

/**
 * @brief 启动指定通道的 PWM 输出。
 *
 * @param channel 通道号，范围 0～3。
 */
void Servo_On(uint8_t channel);

/**
 * @brief 停止指定通道的 PWM 输出。
 *
 * @param channel 通道号，范围 0～3。
 */
void Servo_Off(uint8_t channel);

/**
 * @brief 控制舵机电源。
 *
 * @param enable true 输出高电平，使能舵机电源；
 *               false 输出低电平，关闭舵机电源。
 */
void Servo_Power(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_H */