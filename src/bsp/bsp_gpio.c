/**
 * @file bsp_gpio.c
 * @brief N32G4FR MCU 片上 GPIO 引脚驱动实现
 */
#include "bsp_gpio.h"

#include "n32g4fr.h"

static void bsp_gpio_clock_enable(GPIO_Module* port)
{
    if (port == GPIOA)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    }
    else if (port == GPIOB)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    }
    else if (port == GPIOC)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);
    }
}

void bsp_gpio_init(void)
{
    GPIO_InitType init;

    bsp_gpio_clock_enable((GPIO_Module*)BSP_PWR_HOLD_PORT);

    GPIO_InitStruct(&init);
    init.Pin        = BSP_PWR_HOLD_PIN;
    init.GPIO_Mode  = GPIO_Mode_Out_PP;
    init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitPeripheral((GPIO_Module*)BSP_PWR_HOLD_PORT, &init);

    /* 最先锁定电源保持引脚，防止松开电源键掉电 */
    bsp_power_hold_ctrl(true);
}

/* 使用专用置位/复位寄存器，避免读改写影响同端口的其他输出位。 */
void bsp_gpio_write(void* port, uint16_t pin, bool high)
{
    if (high)
    {
        ((GPIO_Module*)port)->PBSC = pin;
    }
    else
    {
        ((GPIO_Module*)port)->PBC = pin;
    }
}

bool bsp_gpio_read(void* port, uint16_t pin)
{
    return (((GPIO_Module*)port)->PID & pin) != 0U;
}

void bsp_power_hold_ctrl(bool on)
{
    bsp_gpio_write((void*)BSP_PWR_HOLD_PORT, BSP_PWR_HOLD_PIN, on);
}
