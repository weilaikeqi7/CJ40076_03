/**
 * @file app_key.c
 * @brief 按键扫描实现：消抖 + 多击 + 长按 + 双键组合 + 校准页连发
 *
 * 校准页时序：短按 <800ms 单击调节一步；按住 >=800ms 后每 100ms 连发。
 */
#include "app_key.h"

#include "app_config.h"
#include "board.h"

#define SCAN_MS APP_KEY_SCAN_MS

/** 校准页：按住超过该时间开始连发（同时也不再判定为单击） */
#define CALIB_HOLD_MS 800U

typedef struct
{
    bool     raw_last;     /* 上次采样原始电平 */
    bool     stable;       /* 消抖后稳定状态：true=按下 */
    uint16_t debounce_ms;  /* 电平不一致持续计时 */
    uint32_t press_ms;     /* 稳定按下持续计时 */
    bool     long_fired;   /* 长按事件已触发 */
    bool     hold_fired;   /* 校准页连发已启动 */
    uint32_t repeat_ms;    /* 连发计时 */
} key_state_t;

static key_state_t key_power;
static key_state_t key_mode;

static uint8_t  click_count;     /* 模式键多击计数 */
static uint32_t click_expire_ms; /* 多击窗口超时计时 */
static bool     calib_mode;
static bool     both_long_fired;
static uint32_t both_press_ms;
static bool     boot_armed; /* 开机首次按压抑制：松手后才武装 */

static void key_sample(key_state_t* ks, bool raw_down)
{
    if (raw_down != ks->raw_last)
    {
        ks->raw_last    = raw_down;
        ks->debounce_ms = 0U;
    }
    else if (raw_down != ks->stable)
    {
        ks->debounce_ms += SCAN_MS;
        if (ks->debounce_ms >= APP_KEY_DEBOUNCE_MS)
        {
            /* 按下沿才复位计时；松开沿保留 press_ms，
               供扫描逻辑读取本次按压时长（单击/多击判定依赖它） */
            if (!ks->stable && raw_down)
            {
                ks->press_ms   = 0U;
                ks->long_fired = false;
                ks->hold_fired = false;
                ks->repeat_ms  = 0U;
            }
            ks->stable = raw_down;
        }
    }
    else if (ks->stable)
    {
        ks->press_ms  += SCAN_MS;
        ks->repeat_ms += SCAN_MS;
    }
}

void app_key_init(void)
{
    key_power       = (key_state_t){0};
    key_mode        = (key_state_t){0};
    click_count     = 0U;
    click_expire_ms = 0U;
    calib_mode      = false;
    both_press_ms   = 0U;
    both_long_fired = false;
    boot_armed      = false;
}

void app_key_set_calib_mode(bool on)
{
    calib_mode      = on;
    click_count     = 0U;
    click_expire_ms = 0U;
    both_press_ms   = 0U;
    both_long_fired = false;
}

bool app_key_power_down(void)
{
    return key_power.stable;
}

bool app_key_mode_down(void)
{
    return key_mode.stable;
}

/** 校准页按键：单击/连发。raw_evt_short/raw_evt_repeat 为对应事件位 */
static uint16_t calib_key_events(key_state_t* ks, uint16_t evt_short, uint16_t evt_repeat)
{
    uint16_t evt = 0U;

    if (ks->stable)
    {
        if (!ks->hold_fired && ks->press_ms >= CALIB_HOLD_MS)
        {
            ks->hold_fired = true;
            ks->repeat_ms  = 0U;
        }
        if (ks->hold_fired && ks->repeat_ms >= APP_KEY_REPEAT_MS)
        {
            ks->repeat_ms = 0U;
            evt          |= evt_repeat;
        }
    }
    else if (ks->press_ms > 0U)
    {
        /* 已松开：未进入连发才算单击；随后清 press_ms 防止重复上报 */
        if (!ks->hold_fired && ks->press_ms < CALIB_HOLD_MS)
        {
            evt |= evt_short;
        }
        ks->press_ms = 0U;
    }
    return evt;
}

app_key_event_t app_key_scan(void)
{
    app_key_event_t evt = {APP_KEY_EVT_NONE, 0U};

    key_sample(&key_power, board_key_power_pressed());
    key_sample(&key_mode, board_key_mode_pressed());

    /* 开机抑制：电源键首次松开前不产生任何事件 */
    if (!boot_armed)
    {
        if (!key_power.stable)
        {
            boot_armed = true;
        }
        return evt;
    }

    /* ---------- 双键同按（校准页切页/保存） ---------- */
    if (calib_mode && key_power.stable && key_mode.stable)
    {
        both_press_ms += SCAN_MS;
        if (!both_long_fired && both_press_ms >= APP_KEY_BOTH_LONG_MS)
        {
            both_long_fired  = true;
            evt.evt         |= APP_KEY_EVT_BOTH_LONG;
        }
        /* 双键期间屏蔽单击/连发计时 */
        key_power.press_ms = 0U;
        key_mode.press_ms  = 0U;
        return evt;
    }
    both_press_ms = 0U;
    if (!key_power.stable && !key_mode.stable)
    {
        both_long_fired = false;
    }

    if (calib_mode)
    {
        /* ---------- 校准页：单击 + 按住连发 ---------- */
        evt.evt |= calib_key_events(&key_power, APP_KEY_EVT_POWER_SHORT, APP_KEY_EVT_POWER_REPEAT);
        evt.evt |= calib_key_events(&key_mode, APP_KEY_EVT_MODE_SINGLE, APP_KEY_EVT_MODE_REPEAT);
        return evt;
    }

    /* ---------- 正常模式：电源键 ---------- */
    if (key_power.stable)
    {
        if (!key_power.long_fired && key_power.press_ms >= APP_KEY_LONG_MS)
        {
            key_power.long_fired = true;
            evt.evt             |= APP_KEY_EVT_POWER_LONG; /* 到 3s 立即触发 */
        }
    }
    else if (key_power.press_ms > 0U)
    {
        if (key_power.press_ms < APP_KEY_LONG_MS)
        {
            evt.evt |= APP_KEY_EVT_POWER_SHORT;
        }
        key_power.press_ms = 0U;
    }

    /* ---------- 正常模式：模式键多击（最多10击，600ms 窗口） ---------- */
    if (!key_mode.stable && key_mode.press_ms > 0U)
    {
        if (key_mode.press_ms < APP_KEY_LONG_MS && click_count < 10U)
        {
            click_count++;
            click_expire_ms = 0U;
        }
        key_mode.press_ms = 0U;
    }

    if (click_count > 0U && !key_mode.stable)
    {
        click_expire_ms += SCAN_MS;
        if (click_expire_ms >= APP_KEY_MULTICLICK_MS)
        {
            evt.evt        |= APP_KEY_EVT_MODE_CLICKS;
            evt.arg         = click_count;
            click_count     = 0U;
            click_expire_ms = 0U;
        }
    }

    return evt;
}
