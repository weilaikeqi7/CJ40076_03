/**
 * @file bsp_gpio.h
 * @brief N32G4FR MCU 片上 GPIO 引脚驱动（含 PB12 总电源保持引脚）
 *
 * 职责边界：
 *   - 仅封装纯 MCU 片上 GPIO 引脚初始化、电平拨动、上拉下拉与读取；
 *   - 提供 PB12 整机电源自锁保持引脚的开关控制；
 *   - 绝不包含任何外设模块（如显示屏、测距机）的供电逻辑或硬件时序。
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** PB12 总电源保持引脚 */
#define BSP_PWR_HOLD_PORT GPIOB
#define BSP_PWR_HOLD_PIN  GPIO_PIN_12

/**
 * @brief 初始化总电源保持引脚，上电后立即执行以锁定整机供电
 */
void bsp_gpio_init(void);

/**
 * @brief 写指定引脚电平
 * @param port GPIO 端口（如 GPIOA、GPIOB）
 * @param pin  引脚位掩码（如 GPIO_PIN_12，可组合多个位）
 * @note port 必须有效且已开启时钟，目标引脚须预先配置为输出。
 * @param high true 置高，false 置低
 */
void bsp_gpio_write(void* port, uint16_t pin, bool high);

/**
 * @brief 读取指定引脚电平
 * @param port GPIO 端口
 * @param pin  引脚号
 * @return true 引脚为高电平，false 为低电平
 */
bool bsp_gpio_read(void* port, uint16_t pin);

/**
 * @brief 上电自锁保持引脚（PB12）控制
 * @param on true 置高锁定整机供电；false 置低切断整机总电源
 */
void bsp_power_hold_ctrl(bool on);

#ifdef __cplusplus
}
#endif

#endif /* BSP_GPIO_H */
