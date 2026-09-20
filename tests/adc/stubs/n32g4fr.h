#ifndef TEST_N32G4FR_H
#define TEST_N32G4FR_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint16_t Pin; int GPIO_Mode; } GPIO_InitType;
typedef struct { int WorkMode, MultiChEn, ContinueConvEn, ExtTrigSelect, DatAlign, ChsNumber; } ADC_InitType;
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define ADC_CH_0 0U
#define ADC_CH_1 1U
#define GPIOA 0
#define ADC1 0
#define ENABLE 1
#define DISABLE 0
#define SET 1
#define RESET 0
#define RCC_APB2_PERIPH_GPIOA 0
#define GPIO_Mode_AIN 0
#define RCC_AHB_PERIPH_ADC1 0
#define ADC_CTRL3_CKMOD_AHB 0
#define RCC_ADCHCLK_DIV8 0
#define RCC_ADC1MCLK_SRC_HSE 0
#define RCC_ADC1MCLK_DIV8 0
#define ADC_WORKMODE_INDEPENDENT 0
#define ADC_EXT_TRIGCONV_NONE 0
#define ADC_DAT_ALIGN_R 0
#define ADC_FLAG_RDY 0
#define ADC_FLAG_ENDC 0
#define ADC_SAMP_TIME_239CYCLES5 0
extern uint32_t test_adc_control;
#define ADCIP_CTRL test_adc_control
#define RCC_EnableAPB2PeriphClk(a,b) ((void)0)
#define GPIO_InitStruct(a) ((void)(a))
#define GPIO_InitPeripheral(a,b) ((void)(b))
#define RCC_EnableAHBPeriphClk(a,b) ((void)0)
#define ADC_ConfigClk(a,b) ((void)0)
#define RCC_ConfigAdc1mClk(a,b) ((void)0)
#define ADC_InitStruct(a) ((void)(a))
#define ADC_Init(a,b) ((void)(b))
#define ADC_Enable(a,b) ((void)0)
#define ADC_StartCalibration(a) ((void)0)
#define ADC_ClearFlag(a,b) ((void)0)
int ADC_GetFlagStatusNew(int adc, int flag);
int ADC_GetCalibrationStatus(int adc);
void ADC_ConfigRegularChannel(int adc, uint8_t channel, int rank, int sample);
void ADC_EnableSoftwareStartConv(int adc, int enable);
int ADC_GetFlagStatus(int adc, int flag);
uint16_t ADC_GetDat(int adc);
#endif
