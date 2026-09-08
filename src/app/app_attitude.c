/**
 * @file app_attitude.c
 * @brief 姿态解算实现
 */
#include "app_attitude.h"

#include "app_config.h"
#include "jy901b.h"

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
    const jy901b_data_t* imu = jy901b_get_data();
    int32_t              h;

    jy901b_poll();

    if (imu->tick_angle == 0U || imu->tick_angle == last_frame_tick)
    {
        return;
    }
    last_frame_tick = imu->tick_angle;

    /* 俯仰 = -原始俯仰 + PIt */
    pitch_c01 = -(int32_t)(imu->pitch * 100.0f) + offsets.pit_c01;

    /* 航向 = -原始航向 + HIt + HEr，归一化 0~359.99° */
    h = -(int32_t)(imu->yaw * 100.0f) + offsets.hit_c01 + offsets.her_c01;
    h %= 36000;
    if (h < 0)
    {
        h += 36000;
    }
    heading_c01 = h;
}

bool attitude_valid(void)
{
    if (last_frame_tick == 0U)
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
    *out = offsets;
}

void attitude_set_offsets(const app_offsets_t* in)
{
    offsets = *in;
}

void attitude_load_offsets(void)
{
    store_get_offsets(&offsets);
}
