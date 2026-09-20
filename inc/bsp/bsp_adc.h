/**
 * @file bsp_adc.h
 * @brief N32G4FR MCU 片上 ADC 采样驱动（ADC1：PA0=电池分压，PA1=NTC）
 *
 * 职责边界：
 *   - 仅封装片上 ADC 硬件初始化、校准、多通道采样与中位值滤波；
 *   - 不包含电池状态分档或温控闭环等业务算法。
 */
#ifndef BSP_ADC_H
#define BSP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "n32g4fr.h"

/* ADC 通道与引脚定义（PA0=电池分压，PA1=NTC） */
#define BSP_VBAT_ADC_PIN GPIO_PIN_0
#define BSP_VBAT_ADC_CH  ADC_CH_0
#define BSP_NTC_ADC_PIN  GPIO_PIN_1
#define BSP_NTC_ADC_CH   ADC_CH_1

/** 电池分压比：R24=20K 上臂，R27=10K 下臂，VBAT = Vadc * 3 */
#define BSP_VBAT_DIVIDER_NUM 3U
#define BSP_VBAT_DIVIDER_DEN 1U

/** ADC 参考电压（VDD = 3.3V）与分辨率 */
#define BSP_ADC_VREF_MV 3300U
#define BSP_ADC_FULL    4096U

/** NTC 电路上臂电阻（R10 = 10K 接 3V3，NTC 下臂接地） */
#define BSP_NTC_PULLUP_OHM 10000.0f

/** NTC 参数（B 值法，默认 10K@25℃ B=3950，请按实际 NTC 型号修改） */
#define BSP_NTC_R25_OHM 10000.0f
#define BSP_NTC_BETA    3950.0f

void bsp_adc_init(void);

/** 初始化或转换失败标记，不属于有效的 12 位 ADC 采样范围。 */
#define BSP_ADC_INVALID UINT16_MAX

/** 读取 ADC 原始值（0~4095），失败返回 BSP_ADC_INVALID。 */
uint16_t bsp_adc_read_raw(uint8_t channel);
uint16_t bsp_adc_read_filtered(uint8_t channel);

/** 电池电压，单位 mV */
uint32_t bsp_battery_mv(void);

/** NTC 当前阻值，单位 欧姆 */
float bsp_ntc_ohm(void);

/** NTC 传感器故障/开路/短路无效返回值（0.1℃），上层收到后必须强制关断加热防止干烧 */
#define BSP_TEMP_INVALID ((int16_t)-32768)

/** NTC 温度，单位 0.1℃（B 值法换算，传感器异常返回 BSP_TEMP_INVALID） */
int16_t bsp_ntc_temperature_c10(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
