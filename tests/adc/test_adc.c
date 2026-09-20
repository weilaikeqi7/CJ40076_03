#include <assert.h>
#include <stdio.h>
#include "bsp_adc.h"
#include "app_thermal.h"

uint32_t test_adc_control;
static bool ready = true, calibration_busy, timeout;
static unsigned conversions, data_reads, fail_at;
static uint16_t sample = 3500U, heater_duty;
int ADC_GetFlagStatusNew(int adc, int flag) { (void)adc; (void)flag; return ready; }
int ADC_GetCalibrationStatus(int adc) { (void)adc; return calibration_busy; }
void ADC_ConfigRegularChannel(int adc, uint8_t channel, int rank, int cycles)
{ (void)adc; (void)channel; (void)rank; (void)cycles; }
void ADC_EnableSoftwareStartConv(int adc, int enable) { (void)adc; (void)enable; ++conversions; }
int ADC_GetFlagStatus(int adc, int flag)
{ (void)adc; (void)flag; return !timeout && (fail_at == 0U || conversions != fail_at); }
uint16_t ADC_GetDat(int adc) { (void)adc; ++data_reads; return sample; }
void dev_heater_init(void) { heater_duty = 0; }
void dev_heater_set_power(uint16_t duty) { heater_duty = duty; }
void dev_heater_off(void) { heater_duty = 0; }

int main(void)
{
    /* 初始化之前及就绪/校准超时均不得返回采样值。 */
    assert(bsp_adc_read_raw(BSP_NTC_ADC_CH) == BSP_ADC_INVALID);
    ready = false;
    bsp_adc_init();
    assert(bsp_battery_mv() == 0U);
    assert(bsp_ntc_temperature_c10() == BSP_TEMP_INVALID);
    assert(conversions == 0U);
    ready = true;
    calibration_busy = true;
    bsp_adc_init();
    assert(bsp_adc_read_raw(BSP_NTC_ADC_CH) == BSP_ADC_INVALID);
    assert(conversions == 0U);
    calibration_busy = false;
    bsp_adc_init();

    /* 正常采样保留原有滤波、换算和加热行为。 */
    assert(bsp_adc_read_filtered(BSP_NTC_ADC_CH) == 3500U);
    assert(bsp_ntc_temperature_c10() == -101);
    sample = 1655U;
    assert(bsp_battery_mv() == 4000U);
    sample = 3500U;
    app_thermal_step(4000U);
    assert(heater_duty == 350U);

    /* 旧数据仍为低温时转换超时：不能复用旧值维持加热。 */
    timeout = true;
    unsigned old_reads = data_reads;
    assert(bsp_adc_read_raw(BSP_NTC_ADC_CH) == BSP_ADC_INVALID);
    assert(bsp_adc_read_filtered(BSP_NTC_ADC_CH) == BSP_ADC_INVALID);
    assert(bsp_battery_mv() == 0U);
    assert(bsp_ntc_temperature_c10() == BSP_TEMP_INVALID);
    app_thermal_step(4000U);
    assert(heater_duty == 0U && data_reads == old_reads);

    /* 六次滤波中任一次失败都必须上报，不能当作最大值剔除。 */
    timeout = false;
    for (unsigned i = 1; i <= 6; ++i)
    {
        fail_at = conversions + i;
        assert(bsp_adc_read_filtered(BSP_NTC_ADC_CH) == BSP_ADC_INVALID);
    }
    fail_at = 0U;
    assert(bsp_adc_read_filtered(BSP_NTC_ADC_CH) == 3500U);
    sample = 4095U;
    assert(bsp_ntc_temperature_c10() == BSP_TEMP_INVALID);
    sample = 0U;
    assert(bsp_ntc_temperature_c10() == BSP_TEMP_INVALID);
    puts("ADC safety regressions passed");
    return 0;
}
