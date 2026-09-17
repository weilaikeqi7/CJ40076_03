/**
 * @file dev_key.c
 * @brief 面板按键设备驱动实现
 */
#include "dev_key.h"

#include "n32g4fr.h"

#include <stddef.h>

#define DEV_KEY_PORT       GPIOA
#define DEV_KEY_MODE_PIN   GPIO_PIN_4
#define DEV_KEY_POWER_PIN  GPIO_PIN_5

#define DEBOUNCE_TARGET_MS 30U
#define SCAN_STEP_MS       10U

typedef struct
{
    bool     raw_last;
    bool     stable;
    uint16_t debounce_ms;
} key_filter_t;

static key_filter_t filter_power;
static key_filter_t filter_mode;

void dev_key_init(void)
{
    GPIO_InitType init;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);

    GPIO_InitStruct(&init);
    init.Pin        = DEV_KEY_MODE_PIN | DEV_KEY_POWER_PIN;
    init.GPIO_Mode  = GPIO_Mode_IPU; /* 内部上拉输入，低有效 */
    init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitPeripheral(DEV_KEY_PORT, &init);

    filter_power = (key_filter_t){0};
    filter_mode  = (key_filter_t){0};
}

bool dev_key_raw_mode_pressed(void)
{
    return (DEV_KEY_PORT->PID & DEV_KEY_MODE_PIN) == 0U;
}

bool dev_key_raw_power_pressed(void)
{
    return (DEV_KEY_PORT->PID & DEV_KEY_POWER_PIN) == 0U;
}

static void update_filter(key_filter_t* kf, bool raw)
{
    if (raw != kf->raw_last)
    {
        kf->raw_last    = raw;
        kf->debounce_ms = 0U;
    }
    else if (raw != kf->stable)
    {
        kf->debounce_ms += SCAN_STEP_MS;
        if (kf->debounce_ms >= DEBOUNCE_TARGET_MS)
        {
            kf->stable = raw;
        }
    }
}

void dev_key_scan_debounce(bool* power_down, bool* mode_down)
{
    update_filter(&filter_power, dev_key_raw_power_pressed());
    update_filter(&filter_mode, dev_key_raw_mode_pressed());

    if (power_down != NULL)
    {
        *power_down = filter_power.stable;
    }
    if (mode_down != NULL)
    {
        *mode_down = filter_mode.stable;
    }
}
