/**
 * @file dev_display.c
 * @brief P1237 段码屏设备驱动实现（四线串行移位 + BSP TIM3 FR 方波）
 */
#include "dev_display.h"

#include "bsp_power.h"
#include "bsp_timer.h"
#include "n32g4fr.h"

#include <string.h>

#define LCD_DISP_PORT GPIOA
#define LCD_DISP_PIN  GPIO_PIN_6
#define LCD_EI_PORT   GPIOA
#define LCD_EI_PIN    GPIO_PIN_7
#define LCD_LP_PORT   GPIOB
#define LCD_LP_PIN    GPIO_PIN_13

/** 移位时钟高低电平最小保持时间（us），原厂 DEMO 约 2us */
#define LCD_SHIFT_US 2U

/*
 * 字节内位序：默认 LSB=Y240（与 DEMO240.C 一致，每字节 bit0 先移出）。
 * 若实物点亮发现整屏错位/反相，定义 LCD_BIT_REVERSED 改用 MSB=Y240 重试。
 * LCD_COMMON_MASK 为帧缓冲第 1 字节中“有效段”位掩码（剔除 COMMON/NC 位）。
 */
#ifdef LCD_BIT_REVERSED
#define LCD_BIT_MASK(index) ((uint8_t)(0x80U >> ((index) & 0x07U)))
#define LCD_COMMON_MASK     0x3FU /* bit7=Y240(COMMON)、bit6=Y239(NC) */
#else
#define LCD_BIT_MASK(index) ((uint8_t)(1U << ((index) & 0x07U)))
#define LCD_COMMON_MASK     0xFCU /* bit0=Y240(COMMON)、bit1=Y239(NC) */
#endif

/* 全模块共享帧缓冲，无内部互斥；绘制、刷新及电源切换须由上层串行化。 */
static uint8_t lcd_frame[LCD_FRAME_BYTES];

/* 粗略微秒级延时（移位时序要求不高，按 4 周期/次估算） */
static void lcd_delay_us(uint32_t us)
{
    uint32_t count = us * (SystemCoreClock / 1000000U) / 4U;

    while (count-- > 0U)
    {
        __NOP();
    }
}

static void lcd_gpio_write(GPIO_Module* port, uint16_t pin, bool high)
{
    if (high)
    {
        port->PBSC = pin;
    }
    else
    {
        port->PBC = pin;
    }
}

/* FR 交流方波由 BSP 层 TIM3 中断驱动（bsp_timer_fr_start/stop） */

/* ------------------------------ 接口与移位 ------------------------------ */

void lcd_init(void)
{
    GPIO_InitType gpio_init;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_GPIOB, ENABLE);

    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = LCD_DISP_PIN | LCD_EI_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitPeripheral(GPIOA, &gpio_init);

    gpio_init.Pin = LCD_LP_PIN;
    GPIO_InitPeripheral(GPIOB, &gpio_init);

    /* 空闲电平全低，显示禁止 */
    lcd_gpio_write(LCD_DISP_PORT, LCD_DISP_PIN, false);
    lcd_gpio_write(LCD_EI_PORT, LCD_EI_PIN, false);
    lcd_gpio_write(LCD_LP_PORT, LCD_LP_PIN, false);

    lcd_clear();
}

void lcd_power_on(void)
{
    bsp_pwr_lcd(true);
    lcd_delay_us(20000U); /* 等屏驱动电源稳定约 20ms */

    lcd_clear();
    lcd_flush();

    bsp_timer_fr_start();
    lcd_gpio_write(LCD_DISP_PORT, LCD_DISP_PIN, true); /* 开显示 + 背光 */
}

void lcd_power_off(void)
{
    /* 必须先关显示驱动输出，再断屏电源 */
    lcd_gpio_write(LCD_DISP_PORT, LCD_DISP_PIN, false);
    lcd_delay_us(1000U);
    bsp_timer_fr_stop();
    lcd_gpio_write(LCD_EI_PORT, LCD_EI_PIN, false);
    lcd_gpio_write(LCD_LP_PORT, LCD_LP_PIN, false);
    bsp_pwr_lcd(false);
}

void lcd_clear(void)
{
    memset(lcd_frame, 0x00, sizeof(lcd_frame));
}

void lcd_fill(void)
{
    memset(lcd_frame, 0xFF, sizeof(lcd_frame));
    lcd_frame[0] &= LCD_COMMON_MASK; /* COMMON/NC 位恒 0 */
}

void lcd_set_raw(uint8_t y_pin, bool on)
{
    uint16_t index;
    uint8_t  mask;

    if (y_pin == 0U || y_pin > LCD_PIN_COUNT)
    {
        return;
    }

    /* 帧缓冲第 1 字节对应 Y240..Y233，最后 1 字节对应 Y8..Y1 */
    index = (uint16_t)(LCD_PIN_COUNT - y_pin);
    mask  = LCD_BIT_MASK(index);

    if (on)
    {
        lcd_frame[index >> 3] |= mask;
    }
    else
    {
        lcd_frame[index >> 3] &= (uint8_t)~mask;
    }
    lcd_frame[0] &= LCD_COMMON_MASK; /* COMMON/NC 位恒 0 */
}

void lcd_flush(void)
{
    uint32_t i;
    uint8_t  bit;

    for (i = 0U; i < LCD_FRAME_BYTES; i++)
    {
        uint8_t data = lcd_frame[i];

        for (bit = 0U; bit < 8U; bit++)
        {
            lcd_gpio_write(LCD_EI_PORT, LCD_EI_PIN, (data & 0x01U) != 0U);
            lcd_gpio_write(LCD_LP_PORT, LCD_LP_PIN, true);
            lcd_delay_us(LCD_SHIFT_US);
            lcd_gpio_write(LCD_LP_PORT, LCD_LP_PIN, false);
            lcd_delay_us(LCD_SHIFT_US);
            data >>= 1;
        }
    }
}

uint8_t* lcd_frame_buffer(void)
{
    return lcd_frame;
}
