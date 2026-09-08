/**
 * @file app_measure.c
 * @brief 测距轮次状态机实现
 */
#include "app_measure.h"

#include "app_config.h"
#include "ranger.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

typedef enum
{
    ST_IDLE = 0,     /* 无测量 */
    ST_ROUND,        /* 轮次进行中（等回包/聚合） */
    ST_SESSION_WAIT, /* 连续/测试：轮间等待下一周期 */
} meas_state_t;

static meas_state_t     state = ST_IDLE;
static meas_mode_t      mode  = MEAS_MODE_SINGLE;
static measure_result_t result;
static bool             published_flag;

static uint32_t round_start_tick; /* 本轮发起时刻 */
static uint32_t last_frame_tick;  /* 本轮最近回包时刻 */
static uint32_t next_round_tick;  /* 下一轮计划时刻 */
static bool     any_frame;        /* 本轮收到过有效距离帧 */
static uint8_t  frame_count;      /* 本轮有效帧数 */
static uint8_t  max_target_no;    /* 本轮最大目标编号 */

static void round_begin(void)
{
    /* 每轮均先设置多目标模式，再发单次测距 */
    ranger_set_target_mode(RANGER_TARGET_MULTI);
    ranger_range_single();

    state            = ST_ROUND;
    round_start_tick = xTaskGetTickCount();
    last_frame_tick  = round_start_tick;
    any_frame        = false;
    frame_count      = 0U;
    max_target_no    = 0U;

    memset(&result, 0, sizeof(result));
}

static void round_publish(void)
{
    result.publish_tick = xTaskGetTickCount();
    published_flag      = true;
    state               = ST_IDLE;

    LOGI("range: publish near=%s%u mm far=%s%u mm\r\n", result.near_valid ? "" : "-- ",
         result.near_valid ? (unsigned int)result.near_mm : 0U, result.far_valid ? "" : "-- ",
         result.far_valid ? (unsigned int)result.far_mm : 0U);

    /* 连续/测试：排定下一轮 */
    if (mode == MEAS_MODE_CONT)
    {
        state          = ST_SESSION_WAIT;
        next_round_tick = round_start_tick + pdMS_TO_TICKS(APP_MEASURE_CONT_PERIOD_MS);
    }
    else if (mode == MEAS_MODE_TEST)
    {
        state          = ST_SESSION_WAIT;
        next_round_tick = round_start_tick + pdMS_TO_TICKS(APP_MEASURE_TEST_PERIOD_MS);
    }
}

static void round_aggregate_frame(const ranger_range_t* fr)
{
    uint32_t dist_mm = (uint32_t)(fr->distance_m * 1000.0f + 0.5f);
    uint8_t  st_low  = fr->status & 0x0FU;

    if (st_low == 0x04U)
    {
        return; /* 超距帧不改变聚合（静默后按当前聚合结果发布） */
    }

    frame_count++;
    any_frame       = true;
    last_frame_tick = xTaskGetTickCount();

    if (fr->target_no > max_target_no)
    {
        max_target_no = fr->target_no;
    }

    /* 首目标：单目标/有前目标/多目标编号最小 */
    if (!result.near_valid || dist_mm < result.near_mm)
    {
        result.near_valid = true;
        result.near_mm    = dist_mm;
    }

    /* 末目标：有后目标帧，或多目标编号 > 0 的最远帧 */
    if (st_low == 0x02U || st_low == 0x03U || fr->target_no > 0U)
    {
        if (!result.far_valid || dist_mm > result.far_mm)
        {
            result.far_valid = true;
            result.far_mm    = dist_mm;
        }
    }
}

void measure_set_mode(meas_mode_t new_mode)
{
    mode  = new_mode;
    state = ST_IDLE;
    memset(&result, 0, sizeof(result));
    published_flag = false;
}

void measure_stop(void)
{
    measure_set_mode(mode);
}

void measure_trigger(void)
{
    switch (mode)
    {
    case MEAS_MODE_SINGLE:
    case MEAS_MODE_MULTI:
        /* 空闲启动；测量中重按 = 重新发起 */
        round_begin();
        break;

    case MEAS_MODE_CONT:
    case MEAS_MODE_TEST:
        if (state == ST_IDLE)
        {
            round_begin(); /* 立即首轮 */
        }
        else
        {
            /* 会话中短按 = 立即停止（进行中的轮一并取消，保留已发布结果） */
            state = ST_IDLE;
            LOGI("range: session stopped\r\n");
        }
        break;

    default:
        break;
    }
}

void measure_poll(void)
{
    ranger_range_t fr;
    uint32_t       now;

    ranger_poll();

    /* 收集本轮回包 */
    while (ranger_get_range(&fr))
    {
        if (state == ST_ROUND)
        {
            round_aggregate_frame(&fr);
        }
        /* 非轮次期间的游离帧（如残留应答）直接丢弃 */
    }

    if (state != ST_ROUND)
    {
        if (state == ST_SESSION_WAIT)
        {
            now = xTaskGetTickCount();
            if ((int32_t)(now - next_round_tick) >= 0)
            {
                round_begin();
            }
        }
        return;
    }

    now = xTaskGetTickCount();

    /* 帧间静默 200ms -> 聚合发布 */
    if (any_frame && (now - last_frame_tick) >= pdMS_TO_TICKS(APP_MEASURE_SILENCE_MS))
    {
        /* 单目标（仅 1 帧且无后目标/编号 0）：只保留首目标 */
        if (frame_count <= 1U && max_target_no == 0U)
        {
            result.far_valid = false;
        }
        /* 首末同值时末目标无意义 */
        if (result.far_valid && result.near_valid && result.far_mm == result.near_mm)
        {
            result.far_valid = false;
        }
        round_publish();
        return;
    }

    /* 单轮 3s 超时：发布无目标 */
    if ((now - round_start_tick) >= pdMS_TO_TICKS(APP_MEASURE_TIMEOUT_MS))
    {
        LOGI("range: timeout, frames=%u\r\n", (unsigned int)frame_count);
        result.near_valid = false;
        result.far_valid  = false;
        round_publish();
    }
}

bool measure_is_running(void)
{
    return state != ST_IDLE;
}

bool measure_round_active(void)
{
    return state == ST_ROUND;
}

const measure_result_t* measure_get_result(void)
{
    return &result;
}

bool measure_take_published(void)
{
    bool f          = published_flag;
    published_flag  = false;
    return f;
}
