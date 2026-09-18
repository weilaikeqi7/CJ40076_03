/**
 * @file app_calib.c
 * @brief Model-neutral hardware calibration and host compensation state machine.
 */
#include "app_calib.h"

#include "app_attitude.h"
#include "app_config.h"
#include "dev_compass.h"
#include "rtt_log.h"

typedef enum
{
    CMD_NONE = 0,
    CMD_MAG_START,
    CMD_MAG_END,
    CMD_MAG_SAMPLE,
    CMD_ACCEL,
    CMD_ANGLE_REF,
    CMD_FACTORY,
} calib_command_t;

static calib_state_t state = CALIB_NONE;
static calib_command_t pending;
static bool async_started;
static app_offsets_t work;

static bool hardware_busy(void)
{
    return state == CALIB_ACC_BUSY || state == CALIB_ANG_BUSY ||
           state == CALIB_FACTORY_BUSY;
}

static int16_t* page_ptr(void)
{
    switch (state)
    {
    case CALIB_PIT: return &work.pit_c01;
    case CALIB_HIT: return &work.hit_c01;
    case CALIB_HER: return &work.her_c01;
    default: return &work.her_c01;
    }
}

static int16_t page_max(void)
{
    return state == CALIB_PIT ? APP_PIT_MAX_C01 : APP_HIT_MAX_C01;
}

static void page_enter(calib_state_t page)
{
    attitude_get_offsets(&work);
    state = page;
    app_key_set_calib_mode(true);
    LOGI("calib: enter compensation page %d\r\n", (int)page);
}

static void page_exit(void)
{
    (void)store_save_offsets(&work);
    state = CALIB_NONE;
    app_key_set_calib_mode(false);
}

static void page_adjust(int16_t delta_c01)
{
    int16_t* value = page_ptr();
    int32_t next = (int32_t)*value + delta_c01;
    int16_t limit = page_max();

    if (next > limit) next = limit;
    if (next < -limit) next = -limit;
    *value = (int16_t)next;
    attitude_set_offsets(&work);
}

bool calib_handle_key(const app_key_event_t* evt)
{
    /* app handles long shutdown before dispatching here. */
    if (pending != CMD_NONE || hardware_busy() || compass_is_busy()) return true;

    if (state == CALIB_NONE)
    {
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) == 0U) return false;

        switch (evt->arg)
        {
        case 4U:
            page_enter(CALIB_HER);
            return true;
        case 5U:
            state = CALIB_MAG;
            pending = CMD_MAG_START;
            return true;
        case 6U:
            return true;
        case 7U:
            page_enter(CALIB_PIT);
            return true;
        case 8U:
            if (!compass_mag_uses_samples())
            {
                state = CALIB_ACC_BUSY;
                pending = CMD_ACCEL;
            }
            return true;
        case 9U:
            if (!compass_mag_uses_samples())
            {
                state = CALIB_ANG_BUSY;
                pending = CMD_ANGLE_REF;
            }
            return true;
        case 10U:
            state = CALIB_FACTORY_BUSY;
            pending = CMD_FACTORY;
            return true;
        default:
            return false;
        }
    }

    if (state == CALIB_MAG)
    {
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U && evt->arg == 6U)
        {
            pending = CMD_MAG_END;
        }
        else if ((evt->evt & APP_KEY_EVT_POWER_SHORT) != 0U &&
                 compass_mag_uses_samples() && !compass_get_cal_state()->score_valid)
        {
            pending = CMD_MAG_SAMPLE;
        }
        return true;
    }

    if ((evt->evt & APP_KEY_EVT_BOTH_LONG) != 0U)
    {
        if (state == CALIB_PIT)
        {
            state = CALIB_HIT;
        }
        else
        {
            page_exit();
        }
        return true;
    }

    if ((evt->evt & (APP_KEY_EVT_POWER_SHORT | APP_KEY_EVT_POWER_REPEAT)) != 0U)
    {
        page_adjust(10);
        return true;
    }
    if ((evt->evt & (APP_KEY_EVT_MODE_SINGLE | APP_KEY_EVT_MODE_REPEAT)) != 0U)
    {
        page_adjust(-10);
        return true;
    }
    return true;
}

void calib_step(void)
{
    calib_command_t command;

    compass_step();
    if (compass_is_busy()) return;

    /* app has stopped ranging and applied power before executing this queue. */
    command = pending;
    pending = CMD_NONE;
    switch (command)
    {
    case CMD_MAG_START:
        compass_calib_mag_start();
        LOGI("calib: %s magnetic calibration started\r\n", compass_model_name());
        break;
    case CMD_MAG_END:
        compass_calib_mag_end();
        state = CALIB_NONE;
        LOGI("calib: %s magnetic calibration ended\r\n", compass_model_name());
        break;
    case CMD_MAG_SAMPLE:
        if (!compass_get_cal_state()->score_valid) compass_calib_take_sample();
        break;
    case CMD_ACCEL:
        async_started = compass_calib_accel();
        break;
    case CMD_ANGLE_REF:
        async_started = compass_calib_angle_ref();
        break;
    case CMD_FACTORY:
        async_started = compass_factory_reset();
        break;
    default:
        break;
    }

    if (!hardware_busy() || compass_is_busy()) return;
    if (async_started)
    {
        if (state == CALIB_FACTORY_BUSY)
        {
            work.pit_c01 = APP_DEFAULT_PIT_C01;
            work.hit_c01 = APP_DEFAULT_HIT_C01;
            work.her_c01 = APP_DEFAULT_HER_C01;
            (void)store_save_offsets(&work);
            attitude_set_offsets(&work);
        }
        LOGI("calib: %s hardware operation %d finished\r\n", compass_model_name(), (int)state);
    }
    else
    {
        LOGW("calib: %s hardware operation %d rejected\r\n", compass_model_name(), (int)state);
    }
    async_started = false;
    state = CALIB_NONE;
}

calib_state_t calib_get_state(void) { return state; }

int16_t calib_page_value_c01(void)
{
    return (state >= CALIB_PIT && state <= CALIB_HER) ? *page_ptr() : 0;
}

bool calib_mag_in_progress(void) { return state == CALIB_MAG; }

bool calib_page_active(void) { return state != CALIB_NONE; }
