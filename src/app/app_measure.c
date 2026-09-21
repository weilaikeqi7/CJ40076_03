/**
 * @file app_measure.c
 * @brief 传感器任务独占测距状态机；按键任务仅提交请求。
 */
#include "app_measure.h"
#include "app_attitude.h"
#include "app_config.h"
#include "dev_ranger.h"
#include "debug_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

typedef enum
{
    ST_IDLE = 0,
    ST_SETUP,
    ST_COMMAND_WAIT,
    ST_ROUND,
    ST_SESSION_WAIT,
} meas_state_t;

/* 只有固定大小的请求/快照记录跨任务共享，访问时使用短临界区。
 * 邮箱合并尚未处理的按键请求，同时保留连续模式连续按键的启停奇偶性。 */
static meas_mode_t requested_mode = MEAS_MODE_SINGLE;
static bool requested_running;
static bool clear_requested;
static uint32_t request_generation;
static measure_result_t result;
static measure_result_t published_result;
static bool published_flag;
static bool active_snapshot;
static uint32_t round_id_snapshot;

/* 以下轮次工作状态由调用 measure_poll 的传感器任务独占。 */
static meas_state_t state;
static meas_mode_t mode;
static uint32_t generation;
static uint32_t next_round_id;
static measure_result_t working;
static uint32_t round_start_tick;
static uint32_t last_frame_tick;
static uint32_t next_round_tick;
static bool any_frame;
static bool oor_received;
static uint16_t frame_count;
static uint8_t max_target_no;
static const char* publish_reason = "unknown";

static void round_begin(void)
{
    memset(&working, 0, sizeof(working));
    any_frame = false;
    oor_received = false;
    frame_count = 0U;
    max_target_no = 0U;
    state = ST_SETUP;
    DBG_LOGI("[STATE][MEASURE] state=SETUP mode=%u generation=%lu\r\n",
             (unsigned int)mode, (unsigned long)request_generation);
    taskENTER_CRITICAL();
    if (generation == request_generation)
    {
        result = working;
        active_snapshot = true;
    }
    taskEXIT_CRITICAL();
}

static void round_publish(void)
{
    working.publish_tick = xTaskGetTickCount();
    DBG_LOGI("[EVENT][MEASURE] publish mode=%u round=%lu near_valid=%u near_mm=%lu far_valid=%u far_mm=%lu frames=%u reason=%s\r\n",
             (unsigned int)mode, (unsigned long)working.round_id, working.near_valid ? 1U : 0U,
             (unsigned long)working.near_mm, working.far_valid ? 1U : 0U,
             (unsigned long)working.far_mm, (unsigned int)frame_count, publish_reason);
    state = ST_IDLE;
    if (mode == MEAS_MODE_CONT || mode == MEAS_MODE_TEST)
    {
        state = ST_SESSION_WAIT;
        next_round_tick = round_start_tick + pdMS_TO_TICKS(
            mode == MEAS_MODE_CONT ? APP_MEASURE_CONT_PERIOD_MS : APP_MEASURE_TEST_PERIOD_MS);
    }

    /* 解析期间到达的停止/切模式/重触发请求使旧轮次失效。
     * 临界区仅提交固定大小快照，不执行串口、日志、等待或结果计算。 */
    taskENTER_CRITICAL();
    if (generation == request_generation)
    {
        result = working;
        published_result = working;
        published_flag = true;
        active_snapshot = false;
        requested_running = state != ST_IDLE;
    }
    taskEXIT_CRITICAL();
}

static void round_finalize(void)
{
    if ((frame_count <= 1U && max_target_no == 0U) ||
        (working.far_valid && working.near_valid && working.far_mm == working.near_mm))
    {
        working.far_valid = false;
    }
    round_publish();
}

static void round_aggregate_frame(const ranger_range_t* fr)
{
    uint32_t dist_mm;
    uint8_t st_low = fr->status & 0x0FU;
    if (st_low == 0x04U || fr->distance_m < 0.0f)
    {
        oor_received = true;
        return;
    }
    dist_mm = (uint32_t)(fr->distance_m * 1000.0f + 0.5f);
    if (frame_count != UINT16_MAX)
    {
        frame_count++;
    }
    any_frame = true;
    last_frame_tick = fr->tick;
    if (fr->target_no > max_target_no)
    {
        max_target_no = fr->target_no;
    }
    if (!working.near_valid || dist_mm < working.near_mm)
    {
        working.near_valid = true;
        working.near_mm = dist_mm;
    }
    if (st_low == 0x02U || st_low == 0x03U || fr->target_no > 0U)
    {
        if (!working.far_valid || dist_mm > working.far_mm)
        {
            working.far_valid = true;
            working.far_mm = dist_mm;
        }
    }
}

void measure_set_mode(meas_mode_t new_mode)
{
    if ((unsigned int)new_mode > (unsigned int)MEAS_MODE_TEST)
    {
        return;
    }
    taskENTER_CRITICAL();
    requested_mode = new_mode;
    requested_running = false;
    clear_requested = true;
    request_generation++;
    taskEXIT_CRITICAL();
}

void measure_stop(void)
{
    taskENTER_CRITICAL();
    requested_running = false;
    clear_requested = true;
    request_generation++;
    taskEXIT_CRITICAL();
}

void measure_trigger(void)
{
    taskENTER_CRITICAL();
    if (requested_mode == MEAS_MODE_CONT || requested_mode == MEAS_MODE_TEST)
    {
        requested_running = !requested_running;
    }
    else
    {
        requested_running = true;
    }
    if (requested_running)
    {
        active_snapshot = true;
    }
    request_generation++;
    taskEXIT_CRITICAL();
}

void measure_poll(void)
{
    ranger_range_t fr;
    uint32_t now;
    uint32_t requested_generation;
    bool run;
    bool clear;

    taskENTER_CRITICAL();
    requested_generation = request_generation;
    run = requested_running;
    clear = clear_requested;
    clear_requested = false;
    mode = requested_mode;
    if (generation != requested_generation)
    {
        active_snapshot = run;
    }
    if (clear)
    {
        memset(&result, 0, sizeof(result));
        memset(&published_result, 0, sizeof(published_result));
        published_flag = false;
    }
    else if (generation != requested_generation && !run)
    {
        /* 连续/测试会话按键停止时保留上一笔已完成结果；
         * 即使被取消的新轮次已经清空显示槽，也恢复上一笔结果。 */
        result = published_result;
    }
    taskEXIT_CRITICAL();
    if (generation != requested_generation)
    {
        generation = requested_generation;
        state = ST_IDLE;
        ranger_discard_ranges();
        if (run)
        {
            round_begin();
        }
    }

    ranger_poll();
    while (ranger_get_range(&fr))
    {
        if (state == ST_ROUND && !fr.continuous)
        {
            round_aggregate_frame(&fr);
        }
    }

    /* 发送前检查解析期间抢占到达的请求，避免继续推进已取消轮次。 */
    taskENTER_CRITICAL();
    run = generation == request_generation;
    taskEXIT_CRITICAL();
    if (!run)
    {
        return;
    }

    now = xTaskGetTickCount();
    if (state == ST_SESSION_WAIT && (int32_t)(now - next_round_tick) >= 0)
    {
        round_begin();
    }
    if (state == ST_SETUP)
    {
        /* 仅发送固定长度串口指令时暂停任务调度，关闭检查到发送之间的
         * 按键抢占窗口；中断保持开启，20ms 指令间隔绝不暂停调度等待。 */
        vTaskSuspendAll();
        if (generation == request_generation &&
            ranger_try_set_target_mode(RANGER_TARGET_MULTI))
        {
            state = ST_COMMAND_WAIT;
            DBG_LOGI("[STATE][MEASURE] state=COMMAND_WAIT mode=%u\r\n", (unsigned int)mode);
        }
        else
        {
            DBG_LOGW("[FAULT][MEASURE] target_mode_tx_not_ready mode=%u\r\n", (unsigned int)mode);
        }
        (void)xTaskResumeAll();
        return;
    }
    if (state == ST_COMMAND_WAIT)
    {
        /* 姿态同样由传感器任务更新，在实际尝试发送 SINGLE 的边界取快照，
         * 不使用按键时刻或轮次活动标志的上升沿，保证重触发也重新取样。 */
        bool valid;
        int32_t heading;
        int32_t pitch;
        vTaskSuspendAll();
        valid = attitude_valid();
        heading = attitude_heading_c01();
        pitch = attitude_pitch_c01();
        if (generation == request_generation && ranger_try_range_single())
        {
            round_start_tick = xTaskGetTickCount();
            last_frame_tick = round_start_tick;
            next_round_id++;
            if (next_round_id == 0U)
            {
                next_round_id++;
            }
            working.round_id = next_round_id;
            working.request_id = generation;
            working.start_tick = round_start_tick;
            working.mode = mode;
            working.attitude_valid = valid;
            working.heading_c01 = heading;
            working.pitch_c01 = pitch;
            state = ST_ROUND;
            taskENTER_CRITICAL();
            round_id_snapshot = next_round_id;
            taskEXIT_CRITICAL();
        }
        (void)xTaskResumeAll();
        if (state == ST_ROUND)
        {
            DBG_LOGI("[EVENT][MEASURE] single_tx mode=%u round=%lu att_valid=%u heading_c01=%ld pitch_c01=%ld\r\n",
                     (unsigned int)mode, (unsigned long)working.round_id,
                     working.attitude_valid ? 1U : 0U, (long)working.heading_c01,
                     (long)working.pitch_c01);
        }
        return;
    }
    if (state != ST_ROUND)
    {
        return;
    }
    if (oor_received || (any_frame &&
        (now - last_frame_tick) >= pdMS_TO_TICKS(APP_MEASURE_SILENCE_MS)))
    {
        publish_reason = oor_received ? "out_of_range" : "silence";
        round_finalize();
    }
    else if ((now - round_start_tick) >= pdMS_TO_TICKS(APP_MEASURE_TIMEOUT_MS))
    {
        working.near_valid = false;
        working.far_valid = false;
        publish_reason = "timeout";
        round_publish();
    }
}

bool measure_is_running(void)
{
    bool running;
    taskENTER_CRITICAL();
    running = requested_running;
    taskEXIT_CRITICAL();
    return running;
}

bool measure_round_active(void)
{
    bool active;
    taskENTER_CRITICAL();
    active = active_snapshot;
    taskEXIT_CRITICAL();
    return active;
}

uint32_t measure_round_id(void)
{
    uint32_t id;
    taskENTER_CRITICAL();
    id = round_id_snapshot;
    taskEXIT_CRITICAL();
    return id;
}

const measure_result_t* measure_get_result(void)
{
    return &result;
}

void measure_copy_result(measure_result_t* out)
{
    taskENTER_CRITICAL();
    *out = result;
    taskEXIT_CRITICAL();
}

bool measure_take_result(measure_result_t* out)
{
    bool fresh;
    taskENTER_CRITICAL();
    fresh = published_flag;
    if (fresh)
    {
        *out = published_result;
        published_flag = false;
    }
    taskEXIT_CRITICAL();
    return fresh;
}

bool measure_result_is_current(const measure_result_t* snapshot)
{
    bool current;
    taskENTER_CRITICAL();
    current = snapshot->round_id != 0U && snapshot->request_id == request_generation;
    taskEXIT_CRITICAL();
    return current;
}

bool measure_take_published(void)
{
    bool fresh;
    taskENTER_CRITICAL();
    fresh = published_flag;
    published_flag = false;
    taskEXIT_CRITICAL();
    return fresh;
}
