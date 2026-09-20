/**
 * @file dev_heater.h
 * @brief 屏幕加热丝执行器设备驱动（封装 10kHz 高频 PWM 功率输出）
 *
 * 职责边界：
 *   - 面向加热丝执行器件，封装 PWM 占空比输出与开关控制；
 *   - 底层调用 bsp_pwm 驱动，上层供 app_thermal 温控闭环调用。
 */
#ifndef DEV_HEATER_H
#define DEV_HEATER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化加热丝执行器硬件（TIM4_CH1 10kHz PWM）
 */
void dev_heater_init(void);

/**
 * @brief 设置加热丝功率占空比
 * @param permille 请求占空比千分比（0~1000‰），超范围由 BSP 限幅。
 * @note 须先初始化；接口不测量实际功率，也不自行执行温度或欠压保护。
 */
void dev_heater_set_power(uint16_t permille);

/**
 * @brief 强制关闭加热丝输出（0‰）
 */
void dev_heater_off(void);

#ifdef __cplusplus
}
#endif

#endif /* DEV_HEATER_H */
