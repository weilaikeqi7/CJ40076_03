/**
 * @file app_calib.c
 * @brief JY901B 校准与主控姿态补偿设置状态机实现
 */
#include "app_calib.h"

#include "app_attitude.h"
#include "app_config.h"
#include "dev_compass.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

static calib_state_t state = CALIB_NONE;
static app_offsets_t work;

static void ensure_jy901b_on(void)
{
    jy901b_power_ctl(true);
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
    if (state == CALIB_NONE)
    {
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) == 0U) return false;

        switch (evt->arg)
        {
        case 4U:
            ensure_jy901b_on();
            page_enter(CALIB_HER);
            return true;
        case 5U:
            ensure_jy901b_on();
            jy901b_calib_mag_start();
            state = CALIB_MAG;
            LOGI("calib: JY901B magnetic calibration started; rotate each axis\r\n");
            return true;
        case 7U:
            ensure_jy901b_on();
            page_enter(CALIB_PIT);
            return true;
        case 8U:
            ensure_jy901b_on();
            state = CALIB_ACC_BUSY;
            jy901b_calib_accel();
            state = CALIB_NONE;
            LOGI("calib: JY901B accelerometer calibration finished\r\n");
            return true;
        case 9U:
            ensure_jy901b_on();
            state = CALIB_ANG_BUSY;
            jy901b_calib_angle_ref();
            state = CALIB_NONE;
            LOGI("calib: JY901B angle reference finished\r\n");
            return true;
        case 10U:
            ensure_jy901b_on();
            state = CALIB_FACTORY_BUSY;
            jy901b_factory_reset();
            work.pit_c01 = APP_DEFAULT_PIT_C01;
            work.hit_c01 = APP_DEFAULT_HIT_C01;
            work.her_c01 = APP_DEFAULT_HER_C01;
            (void)store_save_offsets(&work);
            attitude_set_offsets(&work);
            state = CALIB_NONE;
            LOGI("calib: JY901B factory reset finished\r\n");
            return true;
        default:
            return true;
        }
    }

    if (state == CALIB_MAG)
    {
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U && evt->arg == 6U)
        {
            jy901b_calib_mag_end();
            state = CALIB_NONE;
            LOGI("calib: JY901B magnetic calibration ended and saved\r\n");
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

calib_state_t calib_get_state(void) { return state; }

int16_t calib_page_value_c01(void)
{
    return (state >= CALIB_PIT && state <= CALIB_HER) ? *page_ptr() : 0;
}

bool calib_mag_in_progress(void) { return state == CALIB_MAG; }

bool calib_page_active(void) { return state != CALIB_NONE; }
