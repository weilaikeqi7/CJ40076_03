/**
 * @file app_power.h
 * @brief 系统电源管理、ICR18650 电池状态监测与安全软关机
 */
#ifndef APP_POWER_H
#define APP_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化电源管理模块状态
 */
void app_power_init(void);

/**
 * @brief 周期性检查电池电量（500ms 周期在 T_PWR 中调用）。
 *        执行滞环比较，更新 4 档电量状态，并在欠压 (<3.0V) 时自动触发关机保护。
 */
void app_power_check(void);

/**
 * @brief 获取当前带载滤波电池电压，单位 mV
 */
uint32_t app_power_get_batt_mv(void);

/**
 * @brief 获取当前电池电量档位（1~4）：
 *        4 = 满电 3 格条 (>=3950mV)
 *        3 = 良好 2 格条 (>=3750mV)
 *        2 = 偏低 1 格条 (>=3550mV)
 *        1 = 濒危 仅外框且1Hz闪烁 (<3550mV)
 */
uint8_t app_power_get_batt_lvl(void);

/** True from the start of shutdown until reset; tasks must not restart devices. */
bool app_power_is_shutting_down(void);

/**
 * @brief 执行整机安全软关机下电流程：
 *        1. 记录关机日志并持久化保存测量累计计数至 Flash；
 *        2. 按照硬件保护顺序依次关闭显示屏(先DISP=0)、测距机、电子罗盘、GNSS及加热丝；
 *        3. 释放 PB12 电源保持脚，切断整机总电源回路并停机。
 */
void app_power_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_POWER_H */
