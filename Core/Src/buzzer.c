/**
 * @file buzzer.c
 * @brief TIM12_CH2（PB15）PWM 蜂鸣器实现。
 */

#include "buzzer.h"

#include "tim.h"

/* 计数时钟：240 MHz / 240 = 1 MHz。 */
#define BUZZER_TIMER_CLOCK 1000000UL

/* 频率范围。 */
#define BUZZER_MIN_HZ 16UL
#define BUZZER_MAX_HZ 500000UL

void Buzzer_Init(void)
{
    /* 预分频 240 - 1，得到 1 MHz 计数时钟。 */
    __HAL_TIM_SET_PRESCALER(&htim12, 240U - 1U);

    /* 默认频率 2 kHz，对应 ARR = 1 MHz / 2 kHz - 1。 */
    __HAL_TIM_SET_AUTORELOAD(&htim12, BUZZER_TIMER_CLOCK / 2000UL - 1U);

    /* 占空比 0，比较值为 0。 */
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0U);

    /* 产生更新事件，使预分频和 ARR 立即生效。 */
    HAL_TIM_GenerateEvent(&htim12, TIM_EVENTSOURCE_UPDATE);

    /* 停止 PWM 输出。 */
    (void)HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_2);
}

void Buzzer_Tone(uint32_t hz)
{
    uint32_t period;

    /* 频率范围检查。 */
    if ((hz < BUZZER_MIN_HZ) || (hz > BUZZER_MAX_HZ))
    {
        return;
    }

    /* 计算一个周期对应的计数值。 */
    period = BUZZER_TIMER_CLOCK / hz;

    /* 设置周期。 */
    __HAL_TIM_SET_AUTORELOAD(&htim12, period - 1U);

    /* 50% 占空比。 */
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, period / 2U);

    /* 产生更新事件，使新的 ARR 和比较值立即生效。 */
    HAL_TIM_GenerateEvent(&htim12, TIM_EVENTSOURCE_UPDATE);

    /* 启动 PWM。 */
    (void)HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);
}

void Buzzer_Off(void)
{
    /* 停止 PWM。 */
    (void)HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_2);

    /* 比较值清零，输出保持低电平。 */
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0U);
}