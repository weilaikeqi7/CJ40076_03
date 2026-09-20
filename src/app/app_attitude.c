/**
 * @file app_attitude.c
 * @brief 按编译时所选罗盘的统一姿态输出叠加主控补偿。
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
    app_offsets_t applied_offsets;
    const compass_data_t* compass = &data;

    compass_poll();
    taskENTER_CRITICAL();
    compass_get_data_snapshot(&data);
    applied_offsets = offsets;
    taskEXIT_CRITICAL();

    if (compass->tick_angle == 0U)
    {
        last_frame_tick = 0U;
        return;
    }
    /* 即使没有新帧，也重新应用补偿值，使补偿页调节在下次轮询立即生效。 */

    /* 驱动已完成安装方向的符号换算；这里只叠加 PIt 并限制到 ±90°。 */
    p = (int32_t)(compass->pitch * 100.0f) + applied_offsets.pit_c01;
    if (p > 9000)
    {
        p = 9000;
    }
    if (p < -9000)
    {
        p = -9000;
    }

    /* 安装方向换算后的航向叠加 HIt、HEr，再归一化到 [0, 360)。 */
    h = (int32_t)(compass->heading * 100.0f) + applied_offsets.hit_c01 + applied_offsets.her_c01;
    h %= APP_HEADING_PERIOD_C01;
    if (h < 0)
    {
        h += APP_HEADING_PERIOD_C01;
    }
    if (h > APP_HEADING_MAX_C01)
    {
        h = APP_HEADING_MAX_C01;
    }
    taskENTER_CRITICAL();
    pitch_c01 = p;
    heading_c01 = h;
    last_frame_tick = compass->tick_angle;
    taskEXIT_CRITICAL();
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
