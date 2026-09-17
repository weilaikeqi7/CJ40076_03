/**
 * @file bsp_timer.h
 * @brief TIM3 周期性中断方波驱动（用于显示屏 FR 交流驱动信号）
 *
 * 职责边界：
 *   - 仅封装片上 TIM3 定时中断配置与指定 GPIO 翻转；
 *   - 不包含显示屏段码刷新、帧缓冲等任何设备层逻辑。
 */
#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** FR 交流方波频率（62Hz，满足 P1237 32~96Hz 要求） */
#define BSP_TIMER_FR_FREQ_HZ 62U

/** FR 输出引脚：PB14 */
#define BSP_TIMER_FR_PORT GPIOB
#define BSP_TIMER_FR_PIN  GPIO_PIN_14

/**
 * @brief 初始化并启动 TIM3 更新中断，按 BSP_TIMER_FR_FREQ_HZ 翻转 FR 引脚
 */
void bsp_timer_fr_start(void);

/**
 * @brief 停止 TIM3 更新中断并将 FR 引脚拉低
 */
void bsp_timer_fr_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TIMER_H */
