/**
 * @file app_calib.h
 * @brief Hardware calibration and common PIt/HIt/HEr compensation state machine.
 *
 * Four clicks: HEr. Seven: PIt, then both keys held: HIt, then save/exit.
 * Five: magnetic calibration. Six: end using the model's save/abort policy.
 * JY901B calibrates continuously; MCP406/MCG505 use short-power-key samples
 * until a score arrives. Only JY901B supports eight/nine clicks (accel/reference).
 * Ten: asynchronous factory reset, followed by host defaults on completion.
 * Busy operations ignore keys; app handles long-power shutdown first.
 */
#ifndef APP_CALIB_H
#define APP_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_key.h"
#include "app_store.h"

typedef enum
{
    CALIB_NONE = 0,
    CALIB_PIT,
    CALIB_HIT,
    CALIB_HER,
    CALIB_MAG,
    CALIB_ACC_BUSY,
    CALIB_ANG_BUSY,
    CALIB_FACTORY_BUSY,
} calib_state_t;

calib_state_t calib_get_state(void);
int16_t calib_page_value_c01(void);
/** Consume calibration keys and queue commands; does not switch device power. */
bool calib_handle_key(const app_key_event_t* evt);
/** Call each T_KEY iteration after stopping ranging and applying power.
 *  Advances queued commands and async operations; reapply power afterwards.
 */
void calib_step(void);
bool calib_mag_in_progress(void);
bool calib_page_active(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIB_H */
