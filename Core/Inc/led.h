/**
 * @file led.h
 * @brief SPI6 WS2812 RGB LED 接口。
 *
 * 硬件：
 *   SPI6_SCK  = PA5
 *   SPI6_MOSI = PA7
 *
 * 数据格式：
 *   24 字节 GRB 颜色 + 32 字节复位。
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "stm32h7xx_ll_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LED。
 *
 * 使能 SPI6，并发送一帧黑色数据，使 LED 熄灭。
 */
void Led_Init(void);

/**
 * @brief 设置 LED 颜色。
 *
 * @param red   红色分量，范围 0～255。
 * @param green 绿色分量，范围 0～255。
 * @param blue  蓝色分量，范围 0～255。
 *
 * 函数按 GRB 顺序编码并阻塞发送 56 字节，耗时约 75 μs。
 */
void Led_SetRGB(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 关闭 LED。
 *
 * 等价于 Led_SetRGB(0, 0, 0)。
 */
void Led_Off(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */