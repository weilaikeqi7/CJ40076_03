/**
 * @file board_adc.c
 * @brief 电池电压 / NTC 温度采样实现（ADC1 软件触发单通道转换）
 */
#include "board_adc.h"

#include "board.h"

#include <math.h>

void board_adc_init(void)
{
    GPIO_InitType gpio_init;
    ADC_InitType adc_init;

    /* PA0 / PA1 模拟输入 */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin       = BOARD_VBAT_ADC_PIN | BOARD_NTC_ADC_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitPeripheral(GPIOA, &gpio_init);

    /* ADC1 时钟：AHB 使能 + HCLK/8（144MHz 下为 18MHz） */
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC1, ENABLE);
    ADC_ConfigClk(ADC_CTRL3_CKMOD_AHB,RCC_ADCHCLK_DIV8);
    RCC_ConfigAdc1mClk(RCC_ADC1MCLK_SRC_HSE, RCC_ADC1MCLK_DIV8);

    ADC_InitStruct(&adc_init);
    adc_init.WorkMode       = ADC_WORKMODE_INDEPENDENT;
    adc_init.MultiChEn      = DISABLE;
    adc_init.ContinueConvEn = DISABLE;
    adc_init.ExtTrigSelect  = ADC_EXT_TRIGCONV_NONE;
    adc_init.DatAlign       = ADC_DAT_ALIGN_R;
    adc_init.ChsNumber      = 1;
    ADC_Init(ADC1, &adc_init);

    /* 上电 -> 等待就绪 -> 自校准 */
    ADC_Enable(ADC1, ENABLE);
    while (ADC_GetFlagStatusNew(ADC1, ADC_FLAG_RDY) == RESET)
    {
    }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET)
    {
    }
}

uint16_t board_adc_read_raw(uint8_t channel)
{
    ADC_ConfigRegularChannel(ADC1, channel, 1, ADC_SAMP_TIME_55CYCLES5);
    ADC_ClearFlag(ADC1, ADC_FLAG_ENDC);
    ADC_EnableSoftwareStartConv(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_ENDC) == RESET)
    {
    }
    return ADC_GetDat(ADC1);
}

uint32_t board_battery_mv(void)
{
    uint32_t raw = board_adc_read_raw(BOARD_VBAT_ADC_CH);

    /* VBAT = raw * Vref / 4096 * (20K + 10K) / 10K */
    return raw * BOARD_ADC_VREF_MV * BOARD_VBAT_DIVIDER_NUM / (BOARD_ADC_FULL * BOARD_VBAT_DIVIDER_DEN);
}

float board_ntc_ohm(void)
{
    uint32_t raw = board_adc_read_raw(BOARD_NTC_ADC_CH);

    if (raw >= (BOARD_ADC_FULL - 1U))
    {
        return 1e9f; /* NTC 开路 */
    }

    /* Rntc = R10 * raw / (4096 - raw) */
    return BOARD_NTC_PULLUP_OHM * (float)raw / (float)(BOARD_ADC_FULL - raw);
}

int16_t board_ntc_temperature_c10(void)
{
    float r = board_ntc_ohm();
    float inv_t;

    if (r <= 0.0f)
    {
        return 0;
    }

    /* B 值法：1/T = 1/T0 + ln(R/R0)/B，T0 = 25℃ = 298.15K */
    inv_t = (1.0f / 298.15f) + (logf(r / BOARD_NTC_R25_OHM) / BOARD_NTC_BETA);
    return (int16_t)((1.0f / inv_t - 273.15f) * 10.0f);
}
