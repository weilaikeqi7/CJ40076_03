/**
 * @file bsp_pwm.c
 * @brief TIM4_CH1 10kHz 高频 PWM 硬件定时器驱动实现
 */
#include "bsp_pwm.h"

#include "n32g4fr.h"

/** 加热丝 PWM 输出引脚：PB6 = TIM4_CH1 */
#define BSP_PWM_PORT GPIOB
#define BSP_PWM_PIN  GPIO_PIN_6

/** 计算预分频时采用的目标步数；实际 ARR 由时钟整除结果决定。 */
#define PWM_STEPS 1000U

/** 计算出的实际 ARR 值 */
static uint16_t pwm_arr;

void bsp_pwm_init(void)
{
    GPIO_InitType        gpio_init;
    TIM_TimeBaseInitType tim_init = {0};
    OCInitType           oc_init;
    RCC_ClocksType       clocks;
    uint32_t             tim_clk;
    uint32_t             psc;
    uint32_t             arr;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM4, ENABLE);

    /* PB6 = TIM4_CH1，默认复用 */
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = BSP_PWM_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitPeripheral(BSP_PWM_PORT, &gpio_init);

    /* APB1 预分频不为 1 时定时器时钟 = PCLK1 * 2（144MHz 系统下为 72MHz） */
    RCC_GetClocksFreqValue(&clocks);
    tim_clk = clocks.Pclk1Freq;
    if (clocks.Pclk1Freq != clocks.HclkFreq)
    {
        tim_clk *= 2U;
    }

    psc = tim_clk / (BSP_PWM_HEATER_FREQ_HZ * PWM_STEPS);
    if (psc == 0U)
    {
        psc = 1U;
    }
    arr     = tim_clk / psc / BSP_PWM_HEATER_FREQ_HZ;
    pwm_arr = (uint16_t)(arr - 1U);

    tim_init.Prescaler = (uint16_t)(psc - 1U);
    tim_init.CntMode   = TIM_CNT_MODE_UP;
    tim_init.Period    = pwm_arr;
    tim_init.ClkDiv    = TIM_CLK_DIV1;
    tim_init.RepetCnt  = 0;
    TIM_InitTimeBase(TIM4, &tim_init);

    TIM_InitOcStruct(&oc_init);
    oc_init.OcMode      = TIM_OCMODE_PWM1;
    oc_init.OutputState = TIM_OUTPUT_STATE_ENABLE;
    oc_init.Pulse       = 0; /* 默认关闭 */
    oc_init.OcPolarity  = TIM_OC_POLARITY_HIGH;
    TIM_InitOc1(TIM4, &oc_init);
    TIM_ConfigOc1Preload(TIM4, TIM_OC_PRE_LOAD_ENABLE);

    TIM_ConfigArPreload(TIM4, ENABLE);
    TIM_Enable(TIM4, ENABLE);
}

void bsp_pwm_set_duty(uint16_t permille)
{
    if (permille > 1000U)
    {
        permille = 1000U;
    }
    TIM_SetCmp1(TIM4, (uint16_t)((uint32_t)pwm_arr * permille / 1000U));
}

void bsp_pwm_set(bool on)
{
    bsp_pwm_set_duty(on ? 1000U : 0U);
}
