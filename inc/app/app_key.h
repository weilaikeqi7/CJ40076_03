/**
 * @file app_key.h
 * @brief 按键驱动：消抖、短按、长按、模式键多击（1~10）、双键组合
 *
 * 事件模型（app_key_scan 每 10ms 调用一次，返回事件位掩码）：
 *   电源键短按        -> APP_KEY_EVT_POWER_SHORT（开机后首次按压被抑制）
 *   电源键长按 3s     -> APP_KEY_EVT_POWER_LONG（按住到 3s 立即触发，不等松手）
 *   模式键 N 击       -> APP_KEY_EVT_MODE_CLICKS，arg = 1~10（末次点击 600ms 后触发）
 *   双键同按 1s       -> APP_KEY_EVT_BOTH_LONG（校准页切页/保存）
 *   校准页按住连调     -> APP_KEY_EVT_POWER_REPEAT / APP_KEY_EVT_MODE_REPEAT（每 100ms）
 *   校准页单击         -> APP_KEY_EVT_POWER_SHORT / APP_KEY_EVT_MODE_SINGLE
 *
 * 校准模式下（app_key_set_calib_mode(true)）：模式键不再累计多击，
 * 单击直接上报 APP_KEY_EVT_MODE_SINGLE，双键长按功能使能。
 */
#ifndef APP_KEY_H
#define APP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_KEY_EVT_NONE         = 0,
    APP_KEY_EVT_POWER_SHORT  = 0x0001,
    APP_KEY_EVT_POWER_LONG   = 0x0002, /* 到 3s 立即触发一次 */
    APP_KEY_EVT_MODE_CLICKS  = 0x0004, /* arg = 击键次数 1~10 */
    APP_KEY_EVT_MODE_SINGLE  = 0x0008, /* 校准页：模式键单击 */
    APP_KEY_EVT_BOTH_LONG    = 0x0010, /* 双键同按 1s */
    APP_KEY_EVT_POWER_REPEAT = 0x0020, /* 校准页：电源键按住连发 */
    APP_KEY_EVT_MODE_REPEAT  = 0x0040, /* 校准页：模式键按住连发 */
} app_key_evt_t;

typedef struct
{
    uint16_t evt; /* app_key_evt_t 位掩码 */
    uint8_t  arg; /* MODE_CLICKS 时为击键次数 */
} app_key_event_t;

void app_key_init(void);

/**
 * @brief 按键扫描（10ms 周期调用）。
 * @return 本周期产生的事件（无事件返回 evt=APP_KEY_EVT_NONE）
 */
app_key_event_t app_key_scan(void);

/**
 * @brief 校准模式开关：true 时模式键单击直报、双键长按使能、连发使能。
 */
void app_key_set_calib_mode(bool on);

/** 当前两键实时状态（已消抖） */
bool app_key_power_down(void);
bool app_key_mode_down(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_H */
