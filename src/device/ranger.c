/**
 * @file ranger.c
 * @brief DYC-15A 激光测距机驱动实现
 */
#include "ranger.h"

#include "board.h"
#include "board_uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define RANGER_UART BOARD_UART_RANGER
#define RANGER_BAUD 115200U /* 出厂默认 */

#define RANGER_HEAD0 0xEEU
#define RANGER_HEAD1 0x16U
#define RANGER_DEV   0x03U

#define RANGER_FRAME_MAX 16U /* EE 16 + LEN + 最多 9 数据 + SUM */

#define RANGER_CMD_GAP_MS 20U /* DYC-15A 逐条处理指令：两条命令间必须留间隔 */

/* 命令码 */
#define RANGER_CMD_SELF_CHECK 0x01U
#define RANGER_CMD_SINGLE     0x02U
#define RANGER_CMD_TARGET     0x03U
#define RANGER_CMD_CONTINUOUS 0x04U
#define RANGER_CMD_STOP       0x05U
#define RANGER_CMD_ERROR      0x06U
#define RANGER_CMD_SET_FREQ   0xA1U
#define RANGER_CMD_GATE_MIN   0xA2U
#define RANGER_CMD_GATE_MAX   0xA4U

#define RANGER_QUEUE_SIZE 4U
static ranger_range_t     range_queue[RANGER_QUEUE_SIZE];
static uint8_t            range_q_head;
static uint8_t            range_q_tail;
static ranger_selfcheck_t selfcheck_result;
static bool               selfcheck_new;
static uint8_t            last_error = 0xFFU;
static uint32_t           last_frame_tick;

/* ------------------------------ 命令发送 ------------------------------ */

static void ranger_send(uint8_t cmd, const uint8_t* params, uint8_t param_len)
{
    static uint32_t last_tx_tick;
    uint8_t frame[RANGER_FRAME_MAX];
    uint8_t len = (uint8_t)(2U + param_len); /* 设备码 + 命令码 + 参数 */
    uint8_t sum = (uint8_t)(RANGER_DEV + cmd);
    uint8_t i;
    uint32_t now = xTaskGetTickCount();

    /* 指令间最小间隔：模块逐条处理，背靠背发送会丢弃后一条（如实测的
       "设多目标 + 单次测距"连发导致测距无响应） */
    if (last_tx_tick != 0U)
    {
        uint32_t elapsed = now - last_tx_tick;

        if (elapsed < pdMS_TO_TICKS(RANGER_CMD_GAP_MS))
        {
            vTaskDelay(pdMS_TO_TICKS(RANGER_CMD_GAP_MS) - elapsed);
        }
    }

    frame[0] = RANGER_HEAD0;
    frame[1] = RANGER_HEAD1;
    frame[2] = len;
    frame[3] = RANGER_DEV;
    frame[4] = cmd;
    for (i = 0U; i < param_len; i++)
    {
        frame[5 + i] = params[i];
        sum          = (uint8_t)(sum + params[i]);
    }
    frame[5 + param_len] = sum;

    board_uart_write(RANGER_UART, frame, (size_t)(6U + param_len));
    last_tx_tick = xTaskGetTickCount();
}

/* ------------------------------ 响应解析 ------------------------------ */

static void ranger_handle_frame(uint8_t cmd, const uint8_t* params, uint8_t param_len)
{
    uint32_t now = xTaskGetTickCount();

    last_frame_tick = now;

    switch (cmd)
    {
    case RANGER_CMD_SINGLE:
    case RANGER_CMD_CONTINUOUS:
        if (param_len >= 4U)
        {
            uint16_t dist_int  = ((uint16_t)params[1] << 8) | params[2];
            uint8_t  dist_frac = params[3];
            ranger_range_t item;

            item.status     = params[0];
            item.target_no  = (uint8_t)(params[0] >> 4); /* 多目标模式有效 */
            item.continuous = (cmd == RANGER_CMD_CONTINUOUS);
            item.tick       = now;

            if (dist_int == 0xFFFFU)
            {
                /* 无效距离（超距/无目标）：状态字节仍有效，须上交给轮次状态机 */
                item.distance_m = -1.0f;
            }
            else
            {
                item.distance_m = (float)dist_int + (float)dist_frac / 10.0f;
            }

            uint8_t next_head = (uint8_t)((range_q_head + 1U) % RANGER_QUEUE_SIZE);
            if (next_head != range_q_tail)
            {
                range_queue[range_q_head] = item;
                range_q_head              = next_head;
            }
        }
        break;

    case RANGER_CMD_SELF_CHECK:
        if (param_len >= 4U)
        {
            uint8_t s1 = params[2];
            uint8_t s0 = params[3];

            selfcheck_result.echo_strength = params[1];
            selfcheck_result.fpga_ok       = (s1 & 0x01U) != 0U;
            selfcheck_result.laser_on      = (s1 & 0x02U) != 0U;
            selfcheck_result.main_wave      = (s1 & 0x04U) != 0U;
            selfcheck_result.echo          = (s1 & 0x08U) != 0U;
            selfcheck_result.bias_on       = (s1 & 0x10U) != 0U;
            selfcheck_result.bias_ok       = (s1 & 0x20U) != 0U;
            selfcheck_result.temp_ok       = (s1 & 0x40U) != 0U;
            selfcheck_result.power_5v6_ok  = (s0 & 0x01U) != 0U;
            selfcheck_result.tick          = now;
            selfcheck_new                  = true;
        }
        break;

    case RANGER_CMD_ERROR:
        if (param_len >= 4U)
        {
            last_error = params[3]; /* Status1 位图，非 0xFF 即有部件异常 */
        }
        break;

    default:
        /* 设置/查询类命令的应答帧（版本、选通距离等）暂不入库 */
        break;
    }
}

void ranger_poll(void)
{
    static uint8_t frame[RANGER_FRAME_MAX];
    static uint8_t index = 0U;
    uint8_t        byte;

    while (board_uart_read(RANGER_UART, &byte, 1U) == 1U)
    {
        /* 帧头同步 */
        if (index == 0U)
        {
            if (byte == RANGER_HEAD0)
            {
                frame[index++] = byte;
            }
            continue;
        }
        if (index == 1U)
        {
            if (byte == RANGER_HEAD1)
            {
                frame[index++] = byte;
            }
            else
            {
                index = 0U;
            }
            continue;
        }

        frame[index++] = byte;

        /* index>=3 后 frame[2] 为数据长度：整帧长 = 2头 + 1长 + LEN + 1校验 */
        if (index >= 3U)
        {
            uint8_t total = (uint8_t)(frame[2] + 4U);

            if (frame[2] < 2U || frame[2] > 9U || total > RANGER_FRAME_MAX)
            {
                index = 0U; /* 长度非法，重新同步 */
                continue;
            }

            if (index >= total)
            {
                uint8_t sum = 0U;
                uint8_t i;

                for (i = 3U; i < (uint8_t)(total - 1U); i++)
                {
                    sum = (uint8_t)(sum + frame[i]);
                }

                if (sum == frame[total - 1U] && frame[3] == RANGER_DEV)
                {
                    ranger_handle_frame(frame[4], &frame[5], (uint8_t)(frame[2] - 2U));
                }
                index = 0U;
            }
        }
    }
}

/* ------------------------------ 对外接口 ------------------------------ */

void ranger_init(void)
{
    board_ranger_power(true);
    board_uart_init(RANGER_UART, RANGER_BAUD);

    /* POWER_ON 拉高后约 1.5s 驱动电容充电完成 */
    vTaskDelay(pdMS_TO_TICKS(1600U));
    board_uart_flush_rx(RANGER_UART);

    range_q_head  = 0U;
    range_q_tail  = 0U;
    selfcheck_new = false;
    last_error    = 0xFFU;
}

void ranger_deinit(void)
{
    ranger_range_stop();
    vTaskDelay(pdMS_TO_TICKS(100U));
    board_ranger_power(false);
}

void ranger_self_check(void)
{
    ranger_send(RANGER_CMD_SELF_CHECK, NULL, 0U);
}

void ranger_range_single(void)
{
    ranger_send(RANGER_CMD_SINGLE, NULL, 0U);
}

void ranger_range_continuous_start(void)
{
    ranger_send(RANGER_CMD_CONTINUOUS, NULL, 0U);
}

void ranger_range_stop(void)
{
    ranger_send(RANGER_CMD_STOP, NULL, 0U);
}

void ranger_set_target_mode(ranger_target_t mode)
{
    ranger_send(RANGER_CMD_TARGET, (const uint8_t[]){(uint8_t)mode}, 1U);
}

void ranger_set_continuous_freq(uint8_t hz)
{
    uint8_t params[2];

    if (hz < 1U)
    {
        hz = 1U;
    }
    if (hz > 10U)
    {
        hz = 10U;
    }
    params[0] = hz;
    params[1] = 0U;
    ranger_send(RANGER_CMD_SET_FREQ, params, 2U);
}

void ranger_set_gate_min(uint16_t m)
{
    uint8_t params[2] = {(uint8_t)(m >> 8), (uint8_t)(m & 0xFFU)};

    ranger_send(RANGER_CMD_GATE_MIN, params, 2U);
}

void ranger_set_gate_max(uint16_t m)
{
    uint8_t params[2] = {(uint8_t)(m >> 8), (uint8_t)(m & 0xFFU)};

    ranger_send(RANGER_CMD_GATE_MAX, params, 2U);
}

bool ranger_get_range(ranger_range_t* out)
{
    if (range_q_head == range_q_tail)
    {
        return false;
    }

    *out         = range_queue[range_q_tail];
    range_q_tail = (uint8_t)((range_q_tail + 1U) % RANGER_QUEUE_SIZE);
    return true;
}

bool ranger_get_selfcheck(ranger_selfcheck_t* out)
{
    bool fresh = selfcheck_new;

    *out          = selfcheck_result;
    selfcheck_new = false;
    return fresh;
}

uint8_t ranger_last_error(void)
{
    return last_error;
}

bool ranger_is_alive(uint32_t timeout_ms)
{
    if (last_frame_tick == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - last_frame_tick) < pdMS_TO_TICKS(timeout_ms);
}
