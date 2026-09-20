#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "app_attitude.h"
#include "app_calib.h"
#include "app_display.h"
#include "app_key.h"
#include "app_measure.h"
#include "app_power.h"
#include "app_store.h"
#include "app_thermal.h"
#include "dev_compass.h"
#include "dev_display.h"
#include "rtt_log.h"

void test_log(const char *format, ...) { (void)format; }
void rtt_log_init(void) {}
void vTaskDelay(uint32_t ticks) { (void)ticks; }
void bsp_adc_init(void) {}
void store_init(void) {}
void app_power_init(void) {}
void app_thermal_init(void) {}
void attitude_load_offsets(void) {}
void attitude_update(void) {}
void display_init(void) {}
void ranger_init(void) {}
void gnss_init(void) {}
void gnss_poll(void) {}
void gnss_power_ctl(bool on) { (void)on; }
void compass_init(void) {}
void compass_step(void) {}
const char *compass_model_name(void) { return "test"; }
bool compass_self_check(uint32_t ms) { (void)ms; return true; }
void app_key_init(void) {}
void bsp_iwdg_init(uint32_t ms) { (void)ms; }
void bsp_iwdg_feed(void) {}
app_key_event_t app_key_scan(void) { return (app_key_event_t){0U, 0U}; }
bool calib_handle_key(const app_key_event_t *event) { (void)event; return false; }
calib_state_t calib_get_state(void) { return CALIB_NONE; }
bool calib_page_active(void) { return false; }
void calib_step(void) {}
bool calib_mag_in_progress(void) { return false; }
void measure_set_mode(meas_mode_t mode) { (void)mode; }
bool measure_is_running(void) { return false; }
void measure_stop(void) {}
void measure_trigger(void) {}
void measure_poll(void) {}
void measure_copy_result(measure_result_t *out) { *out = (measure_result_t){0}; }
bool measure_take_result(measure_result_t *out) { (void)out; return false; }
bool measure_result_is_current(const measure_result_t *snapshot) { (void)snapshot; return false; }
bool measure_round_active(void) { return false; }
uint32_t measure_round_id(void) { return 0U; }
void store_set_count_ram(uint32_t count) { (void)count; }
uint32_t store_get_count(void) { return 0U; }
bool store_save_count(void) { return true; }
void app_power_shutdown(void) {}
bool app_power_is_shutting_down(void) { return false; }
void app_power_check(void) {}
uint8_t app_power_get_batt_lvl(void) { return 0U; }
uint32_t app_power_get_batt_mv(void) { return 0U; }
void compass_power_ctl(bool on) { (void)on; }
void ranger_power_ctl(bool on) { (void)on; }
void lcd_power_off(void) {}
void app_thermal_off(void) {}
bool attitude_valid(void) { return false; }
int32_t attitude_heading_c01(void) { return 0; }
int32_t attitude_pitch_c01(void) { return 0; }
bool coord_get_self(app_geo_point_t *out) { (void)out; return false; }
void coord_compute_target(const app_geo_point_t *self, float distance, int32_t heading,
                          int32_t pitch, app_geo_point_t *out)
{
    (void)self; (void)distance; (void)heading; (void)pitch; (void)out;
}
int16_t calib_page_value_c01(void) { return 0; }
bool compass_mag_uses_samples(void) { return false; }
const compass_cal_state_t *compass_get_cal_state(void) { return (const compass_cal_state_t *)0; }
void compass_get_cal_state_snapshot(compass_cal_state_t *out)
{
    *out = (compass_cal_state_t){0};
}
void display_render(const disp_state_t *state) { (void)state; }
void app_thermal_step(uint32_t mv) { (void)mv; }
