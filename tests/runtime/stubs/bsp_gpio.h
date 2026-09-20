#ifndef TEST_RUNTIME_BSP_GPIO_H
#define TEST_RUNTIME_BSP_GPIO_H
#include <stdbool.h>
#include <stdint.h>
void bsp_gpio_init(void);
void bsp_power_hold_ctrl(bool on);
#endif
