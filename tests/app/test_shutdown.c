#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>

/* Include actual scheduler to exercise its private power policy and heartbeat. */
#include "../../src/app/app.c"

static jmp_buf key_exit;
static jmp_buf shutdown_exit;
static unsigned scans;
static unsigned calibration_steps;
static unsigned power_calls;
static unsigned count_saves;
static unsigned delays;
static bool cancelled;

void test_log(const char* format, ...) { (void)format; }

app_key_event_t app_key_scan(void)
{
    ++scans;
    return (app_key_event_t){APP_KEY_EVT_MODE_CLICKS, 10};
}
bool calib_handle_key(const app_key_event_t* evt) { (void)evt; return true; }
calib_state_t calib_get_state(void) { return CALIB_FACTORY_BUSY; }
bool calib_page_active(void) { return true; }
void calib_step(void) { ++calibration_steps; }
bool calib_mag_in_progress(void)
{
    /* This is the first external call in shutdown: latch must already be set. */
    assert(app_power_is_shutting_down());
    return false;
}
void measure_set_mode(meas_mode_t mode) { (void)mode; }
bool measure_is_running(void) { return false; }
void measure_stop(void) {}
void measure_trigger(void) {}
void store_set_count_ram(uint32_t count) { (void)count; }
uint32_t store_get_count(void) { return 17; }
bool store_save_count(void)
{
    assert(app_power_is_shutting_down());
    ++count_saves;
    return true;
}

void vTaskDelay(uint32_t ticks)
{
    assert(ticks == APP_KEY_SCAN_MS);
    assert(app_power_is_shutting_down());
    assert(s_task_alive_bits & TASK_ALIVE_BIT_KEY);
    s_task_alive_bits = 0;
    if (++delays == 3) longjmp(key_exit, 1);
}

void compass_power_ctl(bool on)
{
    ++power_calls;
    assert(!on && app_power_is_shutting_down());
    cancelled = true;
    /* Model the reported preemption after driver cancellation clears busy. */
    if (setjmp(key_exit) == 0) app_task_key(NULL);
    assert(delays == 3 && scans == 0 && calibration_steps == 0);
    assert(power_calls == 1);
}
void gnss_power_ctl(bool on) { assert(!on && app_power_is_shutting_down()); }
void ranger_power_ctl(bool on) { assert(!on && app_power_is_shutting_down()); }
void lcd_power_off(void) { assert(app_power_is_shutting_down()); }
void app_thermal_off(void) { assert(app_power_is_shutting_down()); }
void bsp_iwdg_feed(void) {}
void bsp_power_hold_ctrl(bool on)
{
    assert(!on && app_power_is_shutting_down());
    longjmp(shutdown_exit, 1);
}

/* Unused scheduler dependencies, kept explicit for native PE linkers. */
void rtt_log_init(void) {}
void bsp_adc_init(void) {}
uint32_t bsp_battery_mv(void) { return 4200; }
void store_init(void) {}
void app_thermal_init(void) {}
void attitude_load_offsets(void) {}
void display_init(void) {}
void ranger_init(void) {}
void gnss_init(void) {}
void compass_init(void) {}
const char* compass_model_name(void) { return "test"; }
bool compass_self_check(uint32_t ms) { (void)ms; return true; }
void app_key_init(void) {}
void bsp_iwdg_init(uint32_t ms) { (void)ms; }
/* 测距与校准快照接口桩：此用例只检查关机后不再推进业务。 */
void app_stop_tasks(void) { assert(app_power_is_shutting_down()); }
void compass_step(void) { ++calibration_steps; }
void measure_copy_result(measure_result_t* out) { *out = (measure_result_t){0}; }
bool measure_take_result(measure_result_t* out) { (void)out; return false; }
bool measure_result_is_current(const measure_result_t* out) { (void)out; return false; }
uint32_t measure_round_id(void) { return 0U; }
void compass_get_cal_state_snapshot(compass_cal_state_t* out) { *out = (compass_cal_state_t){0}; }
void measure_poll(void) {}
void attitude_update(void) {}
void gnss_poll(void) {}
bool measure_round_active(void) { return false; }
bool attitude_valid(void) { return false; }
int32_t attitude_heading_c01(void) { return 0; }
int32_t attitude_pitch_c01(void) { return 0; }
bool measure_take_published(void) { return false; }
const measure_result_t* measure_get_result(void)
{
    static measure_result_t result;
    return &result;
}
bool coord_get_self(app_geo_point_t* out) { (void)out; return false; }
void coord_compute_target(const app_geo_point_t* self, float distance, int32_t heading,
                          int32_t pitch, app_geo_point_t* out)
{
    (void)self; (void)distance; (void)heading; (void)pitch; (void)out;
}
int16_t calib_page_value_c01(void) { return 0; }
bool compass_mag_uses_samples(void) { return false; }
const compass_cal_state_t* compass_get_cal_state(void)
{
    static compass_cal_state_t state;
    return &state;
}
void display_render(const disp_state_t* state) { (void)state; }
void app_thermal_step(uint32_t mv) { (void)mv; }

int main(void)
{
    assert(!app_power_is_shutting_down());
    if (setjmp(shutdown_exit) == 0) app_power_shutdown();
    assert(cancelled && count_saves == 1);
    assert(app_power_is_shutting_down());
    power_apply(); /* Direct callers must also not re-enable the active compass. */
    assert(power_calls == 1 && scans == 0 && calibration_steps == 0);
    printf("app_shutdown model=%d: PASS\n", COMPASS_MODEL);
    return 0;
}
