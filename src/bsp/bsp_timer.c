/**
 * @file bsp_timer.c
 * @brief TIM3 周期性中断方波驱动实现（P1237 显示屏 FR 交流驱动）
 */
#include "bsp_timer.h"

#include "misc.h"
#include "n32g4fr.h"

void bsp_timer_fr_start(void)
{
    RCC_ClocksType       clocks;
    TIM_TimeBaseInitType tim_init = {0};
    NVIC_InitType        nvic_init;
    GPIO_InitType        gpio_init;
    uint32_t             tim_clk;
    uint32_t             psc;
    uint32_t             arr;

    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);

    /* PB14 推挽输出，默认低电平 */
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = BSP_TIMER_FR_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitPeripheral(BSP_TIMER_FR_PORT, &gpio_init);
    BSP_TIMER_FR_PORT->PBC = BSP_TIMER_FR_PIN;

    /* APB1 预分频不为 1 时，定时器时钟 = PCLK1 * 2 */
    RCC_GetClocksFreqValue(&clocks);
    tim_clk = clocks.Pclk1Freq;
    if (clocks.Pclk1Freq != clocks.HclkFreq)
    {
        tim_clk *= 2U;
    }

    /* 计数时钟降到约 1MHz，再按 FR*2 的翻转频率装载 */
    psc = tim_clk / 1000000U;
    if (psc == 0U)
    {
        psc = 1U;
    }
    arr = (tim_clk / psc) / (BSP_TIMER_FR_FREQ_HZ * 2U);
    if (arr == 0U)
    {
        arr = 1U;
    }

    tim_init.Prescaler = (uint16_t)(psc - 1U);
    tim_init.CntMode   = TIM_CNT_MODE_UP;
    tim_init.Period    = (uint16_t)(arr - 1U);
    tim_init.ClkDiv    = TIM_CLK_DIV1;
    tim_init.RepetCnt  = 0;
    TIM_InitTimeBase(TIM3, &tim_init);

    /* 优先级 2：高于 configMAX_SYSCALL_INTERRUPT_PRIORITY(5)，
       ISR 仅翻转 GPIO，不调用 FreeRTOS API */
    nvic_init.NVIC_IRQChannel                   = TIM3_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 2;
    nvic_init.NVIC_IRQChannelSubPriority        = 0;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic_init);

    TIM_ClrIntPendingBit(TIM3, TIM_INT_UPDATE);
    TIM_ConfigInt(TIM3, TIM_INT_UPDATE, ENABLE);
    TIM_Enable(TIM3, ENABLE);
}

void bsp_timer_fr_stop(void)
{
    TIM_Enable(TIM3, DISABLE);
    TIM_ConfigInt(TIM3, TIM_INT_UPDATE, DISABLE);
    BSP_TIMER_FR_PORT->PBC = BSP_TIMER_FR_PIN;
}

/* TIM3 更新中断：仅翻转 FR 引脚，不调用任何 FreeRTOS API */
void TIM3_IRQHandler(void)
{
    if (TIM_GetIntStatus(TIM3, TIM_INT_UPDATE) != RESET)
    {
        TIM_ClrIntPendingBit(TIM3, TIM_INT_UPDATE);
        /* 此处读改写整个输出寄存器；同端口并发写入须由调用方协调。 */
        BSP_TIMER_FR_PORT->POD ^= BSP_TIMER_FR_PIN;
    }
}
