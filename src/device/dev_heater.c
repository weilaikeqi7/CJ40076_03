/**
 * @file dev_heater.c
 * @brief 屏幕加热丝执行器设备驱动实现
 */
#include "dev_heater.h"

#include "bsp_pwm.h"

void dev_heater_init(void)
{
    bsp_pwm_init();
    bsp_pwm_set_duty(0U);
}

void dev_heater_set_power(uint16_t permille)
{
    bsp_pwm_set_duty(permille);
}

void dev_heater_off(void)
{
    bsp_pwm_set_duty(0U);
}
