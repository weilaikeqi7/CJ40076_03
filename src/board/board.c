/**
 * @file board.c
 * @brief CJ40076 V4.0 板级 GPIO 驱动实现
 */
#include "board.h"

/** 加热丝 PWM 分辨率（ARR+1），占空比 = CCR1/1000 */
#define HEATER_PWM_STEPS 1000U

/** heater_pwm_init 计算出的实际 ARR 值 */
static uint16_t heater_pwm_arr;

static void heater_pwm_init(void)
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
    gpio_init.Pin        = BOARD_HEATER_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitPeripheral(BOARD_HEATER_PORT, &gpio_init);

    /* APB1 预分频不为 1 时定时器时钟 = PCLK1 * 2（144MHz 系统下为 72MHz） */
    RCC_GetClocksFreqValue(&clocks);
    tim_clk = clocks.Pclk1Freq;
    if (clocks.Pclk1Freq != clocks.HclkFreq)
    {
        tim_clk *= 2U;
    }

    psc = tim_clk / (BOARD_HEATER_PWM_FREQ_HZ * HEATER_PWM_STEPS);
    if (psc == 0U)
    {
        psc = 1U;
    }
    arr            = tim_clk / psc / BOARD_HEATER_PWM_FREQ_HZ;
    heater_pwm_arr = (uint16_t)(arr - 1U);

    tim_init.Prescaler = (uint16_t)(psc - 1U);
    tim_init.CntMode   = TIM_CNT_MODE_UP;
    tim_init.Period    = heater_pwm_arr;
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

static void enable_gpio_clock(GPIO_Module* gpio)
{
    if (gpio == GPIOA)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    }
    else if (gpio == GPIOB)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    }
    else if (gpio == GPIOC)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);
    }
    else if (gpio == GPIOD)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOD, ENABLE);
    }
    else if (gpio == GPIOE)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOE, ENABLE);
    }
}

static void init_output(GPIO_Module* gpio, uint16_t pin, bool on)
{
    GPIO_InitType gpio_init;

    enable_gpio_clock(gpio);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = pin;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz; /* 开关量，低速即可，降低 EMI */
    GPIO_InitPeripheral(gpio, &gpio_init);

    if (on)
    {
        gpio->PBSC = pin;
    }
    else
    {
        gpio->PBC = pin;
    }
}

static void init_input_pullup(GPIO_Module* gpio, uint16_t pin)
{
    GPIO_InitType gpio_init;

    enable_gpio_clock(gpio);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin       = pin;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitPeripheral(gpio, &gpio_init);
}

void board_power_hold(bool on)
{
    init_output(BOARD_PWR_HOLD_PORT, BOARD_PWR_HOLD_PIN, on);
}

void board_gpio_init(void)
{
    /* 最先保持电源，防止松开电源键后掉电 */
    board_power_hold(true);

    /* 各外设电源默认关闭 */
    init_output(BOARD_PWR_RANGER_PORT, BOARD_PWR_RANGER_PIN, false);
    init_output(BOARD_PWR_JY901B_PORT, BOARD_PWR_JY901B_PIN, false);
    init_output(BOARD_PWR_GNSS_PORT, BOARD_PWR_GNSS_PIN, false);
    init_output(BOARD_PWR_LCD_PORT, BOARD_PWR_LCD_PIN, false);

    /* 加热丝改为 TIM4_CH1 PWM 输出（默认占空比 0） */
    heater_pwm_init();

    /* 按键：低有效，上拉输入 */
    init_input_pullup(BOARD_KEY_MODE_PORT, BOARD_KEY_MODE_PIN);
    init_input_pullup(BOARD_KEY_POWER_PORT, BOARD_KEY_POWER_PIN);
}

void board_ranger_power(bool on)
{
    if (on)
    {
        BOARD_PWR_RANGER_PORT->PBSC = BOARD_PWR_RANGER_PIN;
    }
    else
    {
        BOARD_PWR_RANGER_PORT->PBC = BOARD_PWR_RANGER_PIN;
    }
}

void board_jy901b_power(bool on)
{
    if (on)
    {
        BOARD_PWR_JY901B_PORT->PBSC = BOARD_PWR_JY901B_PIN;
    }
    else
    {
        BOARD_PWR_JY901B_PORT->PBC = BOARD_PWR_JY901B_PIN;
    }
}

void board_gnss_power(bool on)
{
    if (on)
    {
        BOARD_PWR_GNSS_PORT->PBSC = BOARD_PWR_GNSS_PIN;
    }
    else
    {
        BOARD_PWR_GNSS_PORT->PBC = BOARD_PWR_GNSS_PIN;
    }
}

void board_lcd_power(bool on)
{
    if (on)
    {
        BOARD_PWR_LCD_PORT->PBSC = BOARD_PWR_LCD_PIN;
    }
    else
    {
        BOARD_PWR_LCD_PORT->PBC = BOARD_PWR_LCD_PIN;
    }
}

void board_heater_set_duty(uint16_t permille)
{
    if (permille > 1000U)
    {
        permille = 1000U;
    }

    TIM_SetCmp1(TIM4, (uint16_t)((uint32_t)heater_pwm_arr * permille / 1000U));
}

void board_heater(bool on)
{
    board_heater_set_duty(on ? 1000U : 0U);
}

bool board_key_mode_pressed(void)
{
    return (BOARD_KEY_MODE_PORT->PID & BOARD_KEY_MODE_PIN) == 0U;
}

bool board_key_power_pressed(void)
{
    return (BOARD_KEY_POWER_PORT->PID & BOARD_KEY_POWER_PIN) == 0U;
}
