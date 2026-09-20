/**
 * @file bsp_iwdg.h
 * @brief N32G4FR MCU 片上独立看门狗（IWDG）驱动（BSP 层）
 *
 * 职责边界：
 *   - 仅封装片上硬件独立看门狗（LSI 40kHz 独立时钟源）寄存器配置与重装载喂狗；
 *   - 不包含上层业务逻辑，超时周期可在初始化时设定。
 */
#ifndef BSP_IWDG_H
#define BSP_IWDG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化并启动独立看门狗
 * @param timeout_ms 请求超时（毫秒，推荐 2000~3000）；须避免 timeout_ms*625 溢出。
 * @note 重装值限幅为 1~4095；实际周期受 LSI 频差和整数取整影响。
 * @note  一旦使能启动后，硬件上无法通过代码停用，只能依靠周期性喂狗保活或掉电终止。
 */
void bsp_iwdg_init(uint32_t timeout_ms);

/**
 * @brief 重装载看门狗计数器（喂狗，写入 0xAAAA 指令）
 */
void bsp_iwdg_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IWDG_H */
