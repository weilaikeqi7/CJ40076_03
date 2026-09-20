/**
 * @file dev_key.h
 * @brief 面板按键设备驱动（PA4 模式键、PA5 电源键，封装硬件配置与 30ms 滤波消抖）
 *
 * 职责边界：
 *   - 负责两路轻触按键的 GPIO 上拉输入初始化、电平采样与 30ms 防抖滤波；
 *   - 提供稳定的物理按键按下状态，供应用层 app_key 状态机提取单击/长按/多击/双键事件。
 */
#ifndef DEV_KEY_H
#define DEV_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化面板按键引脚（PA4、PA5 上拉输入）
 */
void dev_key_init(void);

/**
 * @brief 读取模式键瞬时物理电平（未消抖）
 * @return true 按下（低电平），false 未按（高电平）
 */
bool dev_key_raw_mode_pressed(void);

/**
 * @brief 读取电源键瞬时物理电平（未消抖）
 * @return true 按下（低电平），false 未按（高电平）
 */
bool dev_key_raw_power_pressed(void);

/**
 * @brief 周期性采样并执行 30ms 稳定消抖滤波（由 10ms 按键任务周期调用）
 * @param[out] power_down 电源键稳定状态，true 为按下；可为 NULL。
 * @param[out] mode_down 模式键稳定状态，true 为按下；可为 NULL。
 * @note 初始化后由单一任务每 10ms 调用；周期改变会改变实际消抖时间。
 */
void dev_key_scan_debounce(bool* power_down, bool* mode_down);

#ifdef __cplusplus
}
#endif

#endif /* DEV_KEY_H */
