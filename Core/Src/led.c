/**
 * @file led.c
 * @brief SPI6 WS2812 RGB LED 实现。
 *
 * WS2812 位编码：
 *   逻辑 0 -> 0x60
 *   逻辑 1 -> 0x78
 *
 * 颜色顺序：G、R、B。
 */

#include "led.h"

#include "main.h"

/* 24 字节颜色 + 32 字节复位。 */
#define LED_BYTES 56U

/* 颜色数据长度。 */
#define LED_COLOR_BYTES 24U

/* 发送缓冲区。 */
static uint8_t s_tx[LED_BYTES];

/**
 * @brief 将一个字节编码为 8 个 SPI 字节。
 *
 * @param value 待编码字节。
 * @param out   输出缓冲区，长度 8 字节。
 */
static void led_encode(uint8_t value, uint8_t *out)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        /* 从最高位开始，1 输出 0x78，0 输出 0x60。 */
        out[bit] = ((value & (0x80U >> bit)) != 0U) ? 0x78U : 0x60U;//0x78长高电平，0x60短高电平
    }
}

void Led_Init(void)
{
    /* 使能 SPI6。 */
    LL_SPI_Enable(SPI6);

    /* 发送黑色，确保上电后 LED 熄灭。 */
    Led_Off();
}

void Led_SetRGB(uint8_t red, uint8_t green, uint8_t blue)
{
    /* 按 GRB 顺序编码 24 字节颜色数据。 */
    led_encode(green, &s_tx[0]);
    led_encode(red, &s_tx[8]);
    led_encode(blue, &s_tx[16]);

    /* 颜色数据之后的 32 字节清零，作为复位信号。 */
    for (uint8_t i = LED_COLOR_BYTES; i < LED_BYTES; ++i)
    {
        s_tx[i] = 0U;
    }

    /* 清除上一次传输完成标志。 */
    LL_SPI_ClearFlag_EOT(SPI6);

    /* 设置传输长度并启动主机传输。 */
    LL_SPI_SetTransferSize(SPI6, LED_BYTES);
    LL_SPI_StartMasterTransfer(SPI6);

    /* 逐字节写入发送寄存器。 */
    for (uint8_t i = 0U; i < LED_BYTES; ++i)
    {
        while (LL_SPI_IsActiveFlag_TXP(SPI6) == 0U)
        {
        }

        LL_SPI_TransmitData8(SPI6, s_tx[i]);
    }

    /* 等待传输完成。 */
    while (LL_SPI_IsActiveFlag_EOT(SPI6) == 0U)
    {
    }

    /* 清除传输完成标志。 */
    LL_SPI_ClearFlag_EOT(SPI6);
}

void Led_Off(void)
{
    Led_SetRGB(0U, 0U, 0U);
}