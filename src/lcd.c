/**
 * @file lcd.c
 * @brief P1237 段码屏底层驱动实现（四线串行移位 + FR 方波定时器）
 */
#include "lcd.h"

#include "board.h"
#include "misc.h"

#include <string.h>

#define LCD_DISP_PORT BOARD_LCD_DISP_PORT
#define LCD_DISP_PIN  BOARD_LCD_DISP_PIN
#define LCD_EI_PORT   BOARD_LCD_EI_PORT
#define LCD_EI_PIN    BOARD_LCD_EI_PIN
#define LCD_LP_PORT   BOARD_LCD_LP_PORT
#define LCD_LP_PIN    BOARD_LCD_LP_PIN
#define LCD_FR_PORT   BOARD_LCD_FR_PORT
#define LCD_FR_PIN    BOARD_LCD_FR_PIN

/** FR 翻转用定时器（APB1 通用定时器） */
#define LCD_FR_TIM TIM3

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

/* ------------------------- FR 方波（TIM3 更新中断） ------------------------- */

static void lcd_fr_start(void)
{
    RCC_ClocksType       clocks;
    TIM_TimeBaseInitType tim_init = {0};
    NVIC_InitType        nvic_init;
    uint32_t             tim_clk;
    uint32_t             psc;
    uint32_t             arr;

    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);

    /* APB1 预分频不为 1 时，定时器时钟 = PCLK1 * 2 */
    RCC_GetClocksFreqValue(&clocks);
    tim_clk = clocks.Pclk1Freq;
    if (clocks.Pclk1Freq != clocks.HclkFreq)
    {
        tim_clk *= 2U;
    }

    /* 计数时钟降到约 1MHz，再按 FR*2 的翻转频率装载 */
    psc = tim_clk / 1000000U;
    if (psc == 0U)
    {
        psc = 1U;
    }
    arr = (tim_clk / psc) / (LCD_FR_FREQ_HZ * 2U);
    if (arr == 0U)
    {
        arr = 1U;
    }

    tim_init.Prescaler = (uint16_t)(psc - 1U);
    tim_init.CntMode   = TIM_CNT_MODE_UP;
    tim_init.Period    = (uint16_t)(arr - 1U);
    tim_init.ClkDiv    = TIM_CLK_DIV1;
    tim_init.RepetCnt  = 0;
    TIM_InitTimeBase(LCD_FR_TIM, &tim_init);

    /* 优先级 2：高于 configMAX_SYSCALL_INTERRUPT_PRIORITY(5)，
       ISR 仅翻转 GPIO，不调用 FreeRTOS API */
    nvic_init.NVIC_IRQChannel                   = TIM3_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 2;
    nvic_init.NVIC_IRQChannelSubPriority        = 0;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic_init);

    TIM_ClrIntPendingBit(LCD_FR_TIM, TIM_INT_UPDATE);
    TIM_ConfigInt(LCD_FR_TIM, TIM_INT_UPDATE, ENABLE);
    TIM_Enable(LCD_FR_TIM, ENABLE);
}

static void lcd_fr_stop(void)
{
    TIM_Enable(LCD_FR_TIM, DISABLE);
    TIM_ConfigInt(LCD_FR_TIM, TIM_INT_UPDATE, DISABLE);
    lcd_gpio_write(LCD_FR_PORT, LCD_FR_PIN, false);
}

void TIM3_IRQHandler(void)
{
    if (TIM_GetIntStatus(LCD_FR_TIM, TIM_INT_UPDATE) != RESET)
    {
        TIM_ClrIntPendingBit(LCD_FR_TIM, TIM_INT_UPDATE);
        LCD_FR_PORT->POD ^= LCD_FR_PIN;
    }
}

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

    gpio_init.Pin = LCD_LP_PIN | LCD_FR_PIN;
    GPIO_InitPeripheral(GPIOB, &gpio_init);

    /* 空闲电平全低，显示禁止 */
    lcd_gpio_write(LCD_DISP_PORT, LCD_DISP_PIN, false);
    lcd_gpio_write(LCD_EI_PORT, LCD_EI_PIN, false);
    lcd_gpio_write(LCD_LP_PORT, LCD_LP_PIN, false);
    lcd_gpio_write(LCD_FR_PORT, LCD_FR_PIN, false);

    lcd_clear();
}

void lcd_power_on(void)
{
    board_lcd_power(true);
    lcd_delay_us(20000U); /* 等屏驱动电源稳定约 20ms */

    lcd_clear();
    lcd_flush();

    lcd_fr_start();
    lcd_gpio_write(LCD_DISP_PORT, LCD_DISP_PIN, true); /* 开显示 + 背光 */
}

void lcd_power_off(void)
{
    /* 必须先关显示驱动输出，再断屏电源 */
    lcd_gpio_write(LCD_DISP_PORT, LCD_DISP_PIN, false);
    lcd_delay_us(1000U);
    lcd_fr_stop();
    lcd_gpio_write(LCD_EI_PORT, LCD_EI_PIN, false);
    lcd_gpio_write(LCD_LP_PORT, LCD_LP_PIN, false);
    board_lcd_power(false);
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
