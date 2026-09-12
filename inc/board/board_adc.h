/**
 * @file board_adc.h
 * @brief 电池电压 / NTC 温度采样（ADC1：PA0=CH0 电池分压，PA1=CH1 NTC）
 */
#ifndef BOARD_ADC_H
#define BOARD_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 电池分压比：R24=20K 上臂，R27=10K 下臂，VBAT = Vadc * 3 */
#define BOARD_VBAT_DIVIDER_NUM 3U
#define BOARD_VBAT_DIVIDER_DEN 1U

/** ADC 参考电压（VDD = 3.3V）与分辨率 */
#define BOARD_ADC_VREF_MV 3300U
#define BOARD_ADC_FULL    4096U

/** NTC 电路上臂电阻（R10 = 10K 接 3V3，NTC 下臂接地） */
#define BOARD_NTC_PULLUP_OHM 10000.0f

/** NTC 参数（B 值法，默认 10K@25℃ B=3950，请按实际 NTC 型号修改） */
#define BOARD_NTC_R25_OHM 10000.0f
#define BOARD_NTC_BETA    3950.0f

void board_adc_init(void);

/** 读取 ADC 原始值（0~4095），channel 取 BOARD_VBAT_ADC_CH / BOARD_NTC_ADC_CH */
uint16_t board_adc_read_raw(uint8_t channel);
uint16_t board_adc_read_filtered(uint8_t channel);

/** 电池电压，单位 mV */
uint32_t board_battery_mv(void);

/** NTC 当前阻值，单位 欧姆 */
float board_ntc_ohm(void);

/** NTC 温度，单位 0.1℃（B 值法换算，参数见上方宏） */
int16_t board_ntc_temperature_c10(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_ADC_H */
