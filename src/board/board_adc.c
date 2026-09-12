/**
 * @file board_adc.c
 * @brief 电池电压 / NTC 温度采样实现（ADC1 软件触发 + MR 稳压切换 + 去极值均值滤波）
 */
#include "board_adc.h"

#include "board.h"

#include <math.h>

/*
 * 国民技术《N32G4x_N32L4x 系列 ADC 使用指南 V1.1.0》第 2.3 节官方推荐：
 * 切换 ADC 模块内核数字供电为内部主稳压器 (MR)，防止 VDDA 纹波导致 ADC 误动或锁死
 */
#define ADCIP_CTRL (*(volatile uint32_t*)(0x40020800U + 0x60U))

void board_adc_init(void)
{
    GPIO_InitType gpio_init;
    ADC_InitType  adc_init;
    uint32_t      timeout;

    /* PA0 / PA1 模拟输入 */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin       = BOARD_VBAT_ADC_PIN | BOARD_NTC_ADC_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitPeripheral(GPIOA, &gpio_init);

    /* 1. 官方推荐：ADC 寄存器初始化前切换数字稳压源为 MR */
    ADCIP_CTRL = 0x28U;

    /* 2. ADC1 时钟配置：AHB 使能 + HCLK/8 (18MHz) + HSE/8 (1MHz 计时时钟) */
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC1, ENABLE);
    ADC_ConfigClk(ADC_CTRL3_CKMOD_AHB, RCC_ADCHCLK_DIV8);
    RCC_ConfigAdc1mClk(RCC_ADC1MCLK_SRC_HSE, RCC_ADC1MCLK_DIV8);

    ADC_InitStruct(&adc_init);
    adc_init.WorkMode       = ADC_WORKMODE_INDEPENDENT;
    adc_init.MultiChEn      = DISABLE;
    adc_init.ContinueConvEn = DISABLE;
    adc_init.ExtTrigSelect  = ADC_EXT_TRIGCONV_NONE;
    adc_init.DatAlign       = ADC_DAT_ALIGN_R;
    adc_init.ChsNumber      = 1;
    ADC_Init(ADC1, &adc_init);

    /* 3. 上电等待 RDY -> 自校准（带超时防死等保护） */
    ADC_Enable(ADC1, ENABLE);
    timeout = 20000U;
    while ((ADC_GetFlagStatusNew(ADC1, ADC_FLAG_RDY) == RESET) && (--timeout > 0U))
    {
    }

    ADC_StartCalibration(ADC1);
    timeout = 20000U;
    while ((ADC_GetCalibrationStatus(ADC1) == SET) && (--timeout > 0U))
    {
    }
}

uint16_t board_adc_read_raw(uint8_t channel)
{
    /* 采用 239.5 周期采样，适配 -40°C 下 400kΩ 高阻抗热敏电阻充分充放电 */
    ADC_ConfigRegularChannel(ADC1, channel, 1, ADC_SAMP_TIME_239CYCLES5);
    ADC_ClearFlag(ADC1, ADC_FLAG_ENDC);
    ADC_EnableSoftwareStartConv(ADC1, ENABLE);

    uint32_t timeout = 50000U;
    while ((ADC_GetFlagStatus(ADC1, ADC_FLAG_ENDC) == RESET) && (--timeout > 0U))
    {
    }

    return ADC_GetDat(ADC1);
}

/* 6点去极值平均滤波：剔除1个最大值和1个最小值，中间4点求均值，彻底消除瞬态尖峰毛刺 */
uint16_t board_adc_read_filtered(uint8_t channel)
{
    uint16_t samples[6];
    uint32_t sum = 0U;
    uint16_t max = 0U;
    uint16_t min = 4096U;
    uint8_t  i;

    for (i = 0U; i < 6U; i++)
    {
        samples[i] = board_adc_read_raw(channel);
        if (samples[i] > max)
        {
            max = samples[i];
        }
        if (samples[i] < min)
        {
            min = samples[i];
        }
        sum += samples[i];
    }

    return (uint16_t)((sum - max - min) / 4U);
}

uint32_t board_battery_mv(void)
{
    uint32_t raw = board_adc_read_filtered(BOARD_VBAT_ADC_CH);

    /* VBAT = raw * Vref / 4096 * (20K + 10K) / 10K */
    return raw * BOARD_ADC_VREF_MV * BOARD_VBAT_DIVIDER_NUM / (BOARD_ADC_FULL * BOARD_VBAT_DIVIDER_DEN);
}

float board_ntc_ohm(void)
{
    uint32_t raw = board_adc_read_filtered(BOARD_NTC_ADC_CH);

    /* 阈值设为 4085（对应约 -55℃ 以下才判开路，避免 -40℃ 下 3997 读数被误判） */
    if (raw >= 4085U)
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
