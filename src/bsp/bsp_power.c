/**
 * @file bsp_power.c
 * @brief 各外设独立供电使能引脚驱动实现
 */
#include "bsp_power.h"
#include "bsp_gpio.h"

#include "n32g4fr.h"

/* PB3 测距机电源开关 */
#define BSP_PWR_RANGER_PORT GPIOB
#define BSP_PWR_RANGER_PIN  GPIO_PIN_3

/* PA8 电子罗盘电源开关 */
#define BSP_PWR_COMPASS_PORT GPIOA
#define BSP_PWR_COMPASS_PIN  GPIO_PIN_8

/* PB15 GNSS 电源开关 */
#define BSP_PWR_GNSS_PORT GPIOB
#define BSP_PWR_GNSS_PIN  GPIO_PIN_15

/* PB4 显示屏电源开关 */
#define BSP_PWR_LCD_PORT GPIOB
#define BSP_PWR_LCD_PIN  GPIO_PIN_4

static void bsp_pwr_pin_init(GPIO_Module* port, uint16_t pin)
{
    GPIO_InitType init;

    if (port == GPIOA)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    }
    else if (port == GPIOB)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    }

    GPIO_InitStruct(&init);
    init.Pin        = pin;
    init.GPIO_Mode  = GPIO_Mode_Out_PP;
    init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitPeripheral(port, &init);

    /* 默认断电 */
    port->PBC = pin;
}

/* 启动阶段调用；重复初始化会使所有外设断电，应由上层统一安排。 */
void bsp_power_init(void)
{
    bsp_pwr_pin_init(BSP_PWR_RANGER_PORT, BSP_PWR_RANGER_PIN);
    bsp_pwr_pin_init(BSP_PWR_COMPASS_PORT, BSP_PWR_COMPASS_PIN);
    bsp_pwr_pin_init(BSP_PWR_GNSS_PORT, BSP_PWR_GNSS_PIN);
    bsp_pwr_pin_init(BSP_PWR_LCD_PORT, BSP_PWR_LCD_PIN);
}

void bsp_pwr_ranger(bool on)
{
    bsp_gpio_write(BSP_PWR_RANGER_PORT, BSP_PWR_RANGER_PIN, on);
}

void bsp_pwr_compass(bool on)
{
    bsp_gpio_write(BSP_PWR_COMPASS_PORT, BSP_PWR_COMPASS_PIN, on);
}

void bsp_pwr_gnss(bool on)
{
    bsp_gpio_write(BSP_PWR_GNSS_PORT, BSP_PWR_GNSS_PIN, on);
}

void bsp_pwr_lcd(bool on)
{
    bsp_gpio_write(BSP_PWR_LCD_PORT, BSP_PWR_LCD_PIN, on);
}
