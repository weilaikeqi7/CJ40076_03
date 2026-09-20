/**
 * @file bsp_pwm.h
 * @brief TIM4_CH1 10kHz 高频 PWM 硬件定时器驱动（加热丝占空比输出）
 *
 * 职责边界：
 *   - 仅封装片上 PWM 定时器硬件初始化与占空比寄存器写入；
 *   - 不包含温度闭环、占空比策略或安全互锁等任何应用业务。
 */
#ifndef BSP_PWM_H
#define BSP_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 加热丝 PWM 频率（10kHz，适应 -40°C 低温高内阻电池，降低纹波冲击） */
#define BSP_PWM_HEATER_FREQ_HZ 10000U

/**
 * @brief 初始化 TIM4_CH1 10kHz PWM 输出（PB6，默认占空比 0）
 */
void bsp_pwm_init(void);

/**
 * @brief 设置加热丝 PWM 占空比
 * @param permille 请求占空比千分比 0~1000，超过 1000 按 1000 处理。
 * @note 须先初始化；比较值按 ARR 缩放，1000 对应 CCR1=ARR，并非严格恒高电平。
 */
void bsp_pwm_set_duty(uint16_t permille);

/**
 * @brief 选择最大请求占空比或关闭（等价 duty 1000/0）。
 * @param on true 设置 1000‰ 请求值，false 设置 0‰；须先初始化。
 */
void bsp_pwm_set(bool on);

#ifdef __cplusplus
}
#endif

#endif /* BSP_PWM_H */
