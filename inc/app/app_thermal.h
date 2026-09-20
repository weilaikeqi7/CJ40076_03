/**
 * @file app_thermal.h
 * @brief 极低温环境（-40°C）屏幕加热与热管理闭环控制
 */
#ifndef APP_THERMAL_H
#define APP_THERMAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化热管理模块（确保加热丝初始处于关闭状态）
 */
void app_thermal_init(void);

/**
 * @brief 极低温自适应闭环温控步进执行（1秒周期调用）。
 *        读取 NTC 温度并结合当前带载电压 vbat_mv 执行阶梯控温与防冲击自保。
 * @param vbat_mv 当前带载滤波电池电压，单位 mV
 * @note 须先初始化 ADC 和热管理模块；无效温度或欠压会在本次调用中关闭加热。
 *       app_thermal_off 不锁定关闭状态，后续 step 可再次开启；关机时须停止周期调用。
 */
void app_thermal_step(uint32_t vbat_mv);

/**
 * @brief 强制关闭加热丝输出（0% 占空比）
 */
void app_thermal_off(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_THERMAL_H */
