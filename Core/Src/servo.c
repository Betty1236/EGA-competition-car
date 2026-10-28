/**
 * @file servo.c
 * @brief TIM1/TIM2 四路 50 Hz 舵机实现。
 */

#include "servo.h"

#include "tim.h"

/* 通道与定时器句柄的对应关系。 */
static TIM_HandleTypeDef *const s_timers[4] =
{
    &htim1,
    &htim1,
    &htim2,
    &htim2
};

/* 通道与定时器通道的对应关系。 */
static const uint32_t s_channels[4] =
{
    TIM_CHANNEL_1,
    TIM_CHANNEL_3,
    TIM_CHANNEL_1,
    TIM_CHANNEL_3
};

void Servo_Init(void)
{
    /* TIM1：1 MHz 计数，20 ms 周期。 */
    __HAL_TIM_SET_PRESCALER(&htim1, 240U - 1U);
    __HAL_TIM_SET_AUTORELOAD(&htim1, 20000U - 1U);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE);

    /* TIM2：1 MHz 计数，20 ms 周期。 */
    __HAL_TIM_SET_PRESCALER(&htim2, 240U - 1U);
    __HAL_TIM_SET_AUTORELOAD(&htim2, 20000U - 1U);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    HAL_TIM_GenerateEvent(&htim2, TIM_EVENTSOURCE_UPDATE);

    /* 四路默认脉宽 1500 μs，并停止输出。 */
    for (uint8_t channel = 0U; channel < 4U; ++channel)
    {
        __HAL_TIM_SET_COMPARE(s_timers[channel],
                              s_channels[channel],
                              1500U);
        (void)HAL_TIM_PWM_Stop(s_timers[channel], s_channels[channel]);
    }

    /* 初始化完成后关闭舵机电源。 */
    Servo_Power(false);
}

void Servo_Set(uint8_t channel, uint16_t pulse_us)
{
    /* 通道号范围和脉宽范围检查。 */
    if ((channel >= 4U) || (pulse_us > 20000U))
    {
        return;
    }

    __HAL_TIM_SET_COMPARE(s_timers[channel], s_channels[channel], pulse_us);
}

void Servo_On(uint8_t channel)
{
    if (channel >= 4U)
    {
        return;
    }

    (void)HAL_TIM_PWM_Start(s_timers[channel], s_channels[channel]);
}

void Servo_Off(uint8_t channel)
{
    if (channel >= 4U)
    {
        return;
    }

    (void)HAL_TIM_PWM_Stop(s_timers[channel], s_channels[channel]);
}

void Servo_Power(bool enable)//PC15是电源开关的控制信号
{
    HAL_GPIO_WritePin(GPIOC,
                      GPIO_PIN_15,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}