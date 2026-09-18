/**
 * @file app_attitude.c
 * @brief Attitude compensation for the compile-time selected compass.
 */
#include "app_attitude.h"

#include "app_config.h"
#include "dev_compass.h"

#include "FreeRTOS.h"
#include "task.h"

static app_offsets_t offsets = {
    .pit_c01 = APP_DEFAULT_PIT_C01,
    .hit_c01 = APP_DEFAULT_HIT_C01,
    .her_c01 = APP_DEFAULT_HER_C01,
};

static int32_t  pitch_c01;
static int32_t  heading_c01;
static uint32_t last_frame_tick;

void attitude_update(void)
{
    int32_t h;
    int32_t p;
    compass_data_t data;
    const compass_data_t* compass = &data;

    compass_poll();
    taskENTER_CRITICAL();
    data = *compass_get_data();
    taskEXIT_CRITICAL();

    if (compass->tick_angle == 0U)
    {
        last_frame_tick = 0U;
        return;
    }
    if (compass->tick_angle == last_frame_tick)
    {
        return;
    }
    last_frame_tick = compass->tick_angle;

    /* Installed pitch convention comes from the driver; add PIt and clamp. */
    p = (int32_t)(compass->pitch * 100.0f) + offsets.pit_c01;
    if (p > 9000)
    {
        p = 9000;
    }
    if (p < -9000)
    {
        p = -9000;
    }
    pitch_c01 = p;

    /* Add HIt + HEr to the installed heading and normalize to [0, 360). */
    h = (int32_t)(compass->heading * 100.0f) + offsets.hit_c01 + offsets.her_c01;
    h %= APP_HEADING_PERIOD_C01;
    if (h < 0)
    {
        h += APP_HEADING_PERIOD_C01;
    }
    if (h > APP_HEADING_MAX_C01)
    {
        h = APP_HEADING_MAX_C01;
    }
    heading_c01 = h;
}

bool attitude_valid(void)
{
    if (last_frame_tick == 0U || !compass_is_alive(APP_IMU_TIMEOUT_MS))
    {
        return false;
    }
    return (xTaskGetTickCount() - last_frame_tick) < pdMS_TO_TICKS(APP_IMU_TIMEOUT_MS);
}

int32_t attitude_pitch_c01(void)
{
    return pitch_c01;
}

int32_t attitude_heading_c01(void)
{
    return heading_c01;
}

void attitude_get_offsets(app_offsets_t* out)
{
    taskENTER_CRITICAL();
    *out = offsets;
    taskEXIT_CRITICAL();
}

void attitude_set_offsets(const app_offsets_t* in)
{
    taskENTER_CRITICAL();
    offsets = *in;
    taskEXIT_CRITICAL();
}

void attitude_load_offsets(void)
{
    store_get_offsets(&offsets);
}
