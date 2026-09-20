#include "app_calib.h"
#include "app_attitude.h"
#include "app_config.h"
#include "dev_compass.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static app_offsets_t offsets = {123, 456, 789};
static app_offsets_t saved;
static compass_cal_state_t cal;
static bool key_calib;
static bool powered;
static bool ranging = true;
static bool busy;
static bool complete;
static bool accept = true;
static unsigned starts;
static unsigned ends;
static unsigned samples;
static unsigned accels;
static unsigned refs;
static unsigned resets;
static unsigned saves;
static unsigned steps;

void test_log(const char* format, ...)
{
    (void)format;
}

const char* compass_model_name(void) { return "host-test"; }
bool compass_mag_uses_samples(void) { return COMPASS_MODEL != COMPASS_MODEL_JY901B; }
const compass_cal_state_t* compass_get_cal_state(void) { return &cal; }
void compass_get_cal_state_snapshot(compass_cal_state_t* out) { *out = cal; }
bool compass_is_busy(void) { return busy; }

static void ready(void)
{
    assert(powered);
    assert(!ranging);
}

void compass_step(void)
{
    ++steps;
    if (busy)
    {
        ready();
        if (complete) busy = false;
    }
}

void compass_calib_mag_start(void)
{
    ready();
    ++starts;
    memset(&cal, 0, sizeof(cal));
}
void compass_calib_mag_end(void) { ready(); ++ends; }
void compass_calib_take_sample(void) { ready(); ++samples; ++cal.sample_count; }

static bool start_async(void)
{
    ready();
    if (!accept) return false;
    complete = false;
    busy = true;
    return true;
}
bool compass_calib_accel(void) { ++accels; return start_async(); }
bool compass_calib_angle_ref(void) { ++refs; return start_async(); }
bool compass_factory_reset(void) { ++resets; return start_async(); }

void attitude_get_offsets(app_offsets_t* out) { *out = offsets; }
void attitude_set_offsets(const app_offsets_t* in) { offsets = *in; }
bool store_save_offsets(const app_offsets_t* in) { saved = *in; ++saves; return true; }
void app_key_set_calib_mode(bool on) { key_calib = on; }

static bool key(uint16_t event, uint8_t arg)
{
    const app_key_event_t evt = {event, arg};
    return calib_handle_key(&evt);
}
static bool clicks(uint8_t count) { return key(APP_KEY_EVT_MODE_CLICKS, count); }

/* Simulate app ordering: handle key, stop ranging, apply power, step, release. */
static void step(void)
{
    if (calib_page_active()) ranging = false;
    powered = calib_page_active();
    calib_step();
    powered = calib_page_active();
}

static void assert_defaults(const app_offsets_t* value)
{
    assert(value->pit_c01 == APP_DEFAULT_PIT_C01);
    assert(value->hit_c01 == APP_DEFAULT_HIT_C01);
    assert(value->her_c01 == APP_DEFAULT_HER_C01);
}

static void test_dispatch_and_magnetic(void)
{
    assert(!clicks(1));
    assert(!clicks(3));
    assert(!key(APP_KEY_EVT_POWER_SHORT, 0));
    assert(clicks(6));
    assert(calib_get_state() == CALIB_NONE);

    assert(clicks(5));
    assert(calib_mag_in_progress());
    assert(calib_page_active());
    assert(starts == 0);
    assert(!powered && ranging);
    assert(!key_calib); /* Six-click detector must remain enabled. */
    assert(clicks(6)); /* Pending start consumes other keys without replacing it. */
    step();
    assert(starts == 1 && ends == 0);
    assert(powered && !ranging);
    assert(calib_mag_in_progress());

    assert(key(APP_KEY_EVT_POWER_SHORT, 0));
    assert(samples == 0); /* Sampling also deferred until step. */
    step();
    assert(samples == (compass_mag_uses_samples() ? 1U : 0U));
    cal.score_valid = true;
    cal.cal_score = 0.42f;
    assert(key(APP_KEY_EVT_POWER_SHORT, 0));
    step();
    assert(samples == (compass_mag_uses_samples() ? 1U : 0U));
    assert(clicks(1));
    assert(clicks(3));
    assert(clicks(6));
    assert(ends == 0 && calib_mag_in_progress());
    step();
    assert(ends == 1 && calib_get_state() == CALIB_NONE && !powered);

    assert(clicks(5));
    step();
    assert(!cal.score_valid);
    assert(clicks(6));
    step();
    assert(ends == 2 && calib_get_state() == CALIB_NONE);
}

static void test_async_keys(void)
{
    for (uint8_t count = 8; count <= 9; ++count)
    {
        unsigned before = accels + refs;
        assert(clicks(count));
        assert(accels + refs == before);
        if (compass_mag_uses_samples())
        {
            assert(calib_get_state() == CALIB_NONE);
            step();
            assert(accels + refs == before && !powered);
            continue;
        }
        calib_state_t expected = count == 8 ? CALIB_ACC_BUSY : CALIB_ANG_BUSY;
        assert(calib_get_state() == expected);
        step();
        assert(accels + refs == before + 1 && busy);
        assert(calib_get_state() == expected && powered);
        for (unsigned i = 0; i < 20; ++i)
        {
            assert(clicks(1));
            assert(clicks(3));
            assert(clicks(10));
            assert(key(APP_KEY_EVT_BOTH_LONG, 0));
            assert(key(APP_KEY_EVT_POWER_SHORT, 0));
            step();
            assert(calib_get_state() == expected && busy);
        }
        assert(resets == 0 && !key_calib);
        complete = true;
        step();
        assert(calib_get_state() == CALIB_NONE && !powered);
    }
}

static void test_factory_completion(void)
{
    unsigned before = saves;
    assert(clicks(10));
    assert(calib_get_state() == CALIB_FACTORY_BUSY && resets == 0);
    assert(offsets.pit_c01 == 123 && saves == before);
    step();
    assert(resets == 1 && busy && powered);
    assert(offsets.pit_c01 == 123 && saves == before);
    step();
    assert(saves == before);
    complete = true;
    step();
    assert(saves == before + 1 && !powered);
    assert_defaults(&offsets);
    assert_defaults(&saved);
    assert(calib_get_state() == CALIB_NONE);

    offsets.pit_c01 = 321;
    accept = false;
    assert(clicks(10));
    step();
    assert(calib_get_state() == CALIB_NONE && !powered);
    assert(saves == before + 1 && offsets.pit_c01 == 321);
    accept = true;
}

static void test_compensation(void)
{
    unsigned before = saves;
    assert(clicks(7));
    assert(key_calib && calib_get_state() == CALIB_PIT);
    int16_t original = offsets.pit_c01;
    assert(key(APP_KEY_EVT_POWER_SHORT, 0));
    assert(offsets.pit_c01 == original + 10);
    assert(key(APP_KEY_EVT_MODE_SINGLE, 0));
    assert(offsets.pit_c01 == original);
    assert(key(APP_KEY_EVT_BOTH_LONG, 0));
    assert(calib_get_state() == CALIB_HIT);
    assert(key(APP_KEY_EVT_BOTH_LONG, 0));
    assert(calib_get_state() == CALIB_NONE && !key_calib && saves == before + 1);
    assert(clicks(4));
    assert(calib_get_state() == CALIB_HER && key_calib);
    assert(key(APP_KEY_EVT_BOTH_LONG, 0));
    assert(calib_get_state() == CALIB_NONE && !key_calib);
    assert(!clicks(1) && !clicks(3));
}

int main(void)
{
    test_dispatch_and_magnetic();
    test_async_keys();
    test_factory_completion();
    test_compensation();
    assert(steps > 0);
    printf("app_calib model=%d: PASS\n", COMPASS_MODEL);
    return 0;
}
