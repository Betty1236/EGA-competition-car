/**
 * @file buzzer.h
 * @brief TIM12_CH2（PB15）PWM 蜂鸣器接口。
 *
 * 硬件：
 *   TIM12_CH2 / PB15
 *   计数时钟 1 MHz
 *
 * 频率范围：16 Hz～500 kHz
 * 占空比：50%
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蜂鸣器。
 *
 * 配置：
 *   预分频 240 - 1，计数时钟 1 MHz；
 *   频率 2 kHz；
 *   占空比 0；
 *   PWM 输出关闭。
 */
void Buzzer_Init(void);

/**
 * @brief 以指定频率启动蜂鸣器。
 *
 * @param hz 频率，范围 16～500000 Hz，占空比固定 50%。
 *
 * 频率超出范围时，函数直接返回，PWM 状态维持原状。
 */
void Buzzer_Tone(uint32_t hz);

/**
 * @brief 停止蜂鸣器输出。
 *
 * PWM 停止后，比较值清零。
 */
void Buzzer_Off(void);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */