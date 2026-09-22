/**
 * @file dev_heater.c
 * @brief 屏幕加热丝执行器设备驱动实现
 */
#include "dev_heater.h"

#include "bsp_pwm.h"
#include "debug_log.h"

void dev_heater_init(void)
{
    bsp_pwm_init();
    bsp_pwm_set_duty(0U);
    LOG_HEATER("[DBG][HEATER] init duty_permille=0\r\n");
}

/* 这里只转换为 PWM 输出；温度有效性与电池电压保护由 app_thermal 决定。 */
void dev_heater_set_power(uint16_t permille)
{
    bsp_pwm_set_duty(permille);
    LOG_HEATER("[DBG][HEATER] pwm_set duty_permille=%u\r\n", (unsigned int)permille);
}

void dev_heater_off(void)
{
    bsp_pwm_set_duty(0U);
    LOG_HEATER("[DBG][HEATER] pwm_off duty_permille=0\r\n");
}
