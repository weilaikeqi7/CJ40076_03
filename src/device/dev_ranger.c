/**
 * @file dev_ranger.c
 * @brief DYC-15A 激光测距机设备驱动实现
 */
#include "dev_ranger.h"

#include "bsp_power.h"
#include "bsp_uart.h"
#include "debug_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define RANGER_UART BSP_UART_RANGER
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

/* 一次串口解析可能收到全部 16 个编号目标；环形队列额外保留一个空槽。 */
#define RANGER_QUEUE_SIZE 17U
static ranger_range_t     range_queue[RANGER_QUEUE_SIZE];
static uint8_t            range_q_head;
static uint8_t            range_q_tail;
static ranger_selfcheck_t selfcheck_result;
static bool               selfcheck_new;
static uint8_t            last_error = 0xFFU;
static uint32_t           last_frame_tick;
static uint32_t           last_tx_tick;
static bool               tx_started;
static uint8_t            rx_frame[RANGER_FRAME_MAX];
static uint8_t            rx_index;
static bool               ranger_powered;

/* ------------------------------ 命令发送 ------------------------------ */

static bool ranger_tx_ready(void)
{
    return !tx_started ||
        (xTaskGetTickCount() - last_tx_tick) >= pdMS_TO_TICKS(RANGER_CMD_GAP_MS);
}

static bool ranger_try_send(uint8_t cmd, const uint8_t* params, uint8_t param_len)
{
    uint8_t frame[RANGER_FRAME_MAX];
    uint8_t len = (uint8_t)(2U + param_len); /* 设备码 + 命令码 + 参数 */
    uint8_t sum = (uint8_t)(RANGER_DEV + cmd);
    uint8_t i;

    if (!ranger_tx_ready())
    {
        return false;
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

    RAW_RANGER_TX(frame, (size_t)(6U + param_len));
    bsp_uart_write(RANGER_UART, frame, (size_t)(6U + param_len));
    last_tx_tick = xTaskGetTickCount();
    tx_started = true;
    return true;
}

/* 保留独立旧命令接口的同步发送语义；测量状态机仅调用可取消的 try 接口，
 * 不通过此等待路径。发送状态由同一调用任务独占，不支持并发发送。 */
static void ranger_send(uint8_t cmd, const uint8_t* params, uint8_t param_len)
{
    while (!ranger_try_send(cmd, params, param_len))
    {
        vTaskDelay(1U);
    }
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

            LOG_RANGER("[DATA][RANGER] cmd=0x%02X status=0x%02X target=%u valid=%u distance=%.1fm\r\n",
                 (unsigned int)cmd, (unsigned int)item.status, (unsigned int)item.target_no,
                 item.distance_m >= 0.0f ? 1U : 0U, item.distance_m);

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

#if ENABLE_DEBUG_LOG
static void ranger_debug_status(void)
{
    static TickType_t last_tick;
    static bool first = true;
    TickType_t now = xTaskGetTickCount();
    uint32_t age_ms = last_frame_tick == 0U
                          ? 0U
                          : (uint32_t)(((uint64_t)(now - last_frame_tick) * 1000U) /
                                       configTICK_RATE_HZ);
    if (!first && (TickType_t)(now - last_tick) < pdMS_TO_TICKS(2000U)) return;
    first = false;
    last_tick = now;
    LOG_RANGER("[STATUS][RANGER] power=%u alive=%u age_ms=%lu uart_rx=%u last_error=0x%02X\r\n",
             ranger_powered ? 1U : 0U, ranger_is_alive(3000U) ? 1U : 0U,
             (unsigned long)age_ms, (unsigned int)bsp_uart_available(RANGER_UART),
             (unsigned int)last_error);
}
#endif

void ranger_poll(void)
{
    uint8_t* frame = rx_frame;
#if ENABLE_DEBUG_LOG
    ranger_debug_status();
#endif
    uint8_t index = rx_index;
    uint8_t byte;

    while (bsp_uart_read(RANGER_UART, &byte, 1U) == 1U)
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

                RAW_RANGER(frame, total);
                if (sum == frame[total - 1U] && frame[3] == RANGER_DEV)
                {
                    ranger_handle_frame(frame[4], &frame[5], (uint8_t)(frame[2] - 2U));
                }
                else
                {
                    LOG_RANGER("[DATA][RANGER] checksum_or_device_invalid\r\n");
                }
                index = 0U;
            }
        }
    }
    rx_index = index;
}

void ranger_discard_ranges(void)
{
    bsp_uart_flush_rx(RANGER_UART);
    rx_index = 0U;
    range_q_head = 0U;
    range_q_tail = 0U;
}

bool ranger_try_set_target_mode(ranger_target_t mode)
{
    uint8_t param = (uint8_t)mode;
    return ranger_try_send(RANGER_CMD_TARGET, &param, 1U);
}

bool ranger_try_range_single(void)
{
    if (!ranger_tx_ready())
    {
        return false;
    }
    /* 清掉旧指令的缓存字节/半帧，防止污染新轮次。协议不带事务编号，
     * 新指令发出后才到达的旧应答无法在软件中无歧义归属。 */
    ranger_discard_ranges();
    return ranger_try_send(RANGER_CMD_SINGLE, NULL, 0U);
}

/* ------------------------------ 对外接口 ------------------------------ */

void ranger_init(void)
{
    bsp_pwr_ranger(true);
    bsp_uart_init(RANGER_UART, RANGER_BAUD);

    /* POWER_ON 拉高后约 1.5s 驱动电容充电完成 */
    vTaskDelay(pdMS_TO_TICKS(1600U));
    bsp_uart_flush_rx(RANGER_UART);

    range_q_head  = 0U;
    range_q_tail  = 0U;
    selfcheck_new = false;
    last_error    = 0xFFU;
    last_frame_tick = 0U;
    tx_started = false;
    last_tx_tick = 0U;
    rx_index = 0U;
    ranger_powered = true;
    LOG_RANGER("[STATE][RANGER] power=1 baud=%u\r\n", (unsigned int)RANGER_BAUD);
}

void ranger_power_ctl(bool on)
{
    bsp_pwr_ranger(on);
    ranger_powered = on;
    LOG_RANGER("[STATE][RANGER] power=%u\r\n", on ? 1U : 0U);
}

void ranger_deinit(void)
{
    ranger_range_stop();
    vTaskDelay(pdMS_TO_TICKS(100U));
    bsp_pwr_ranger(false);
    ranger_powered = false;
    LOG_RANGER("[STATE][RANGER] power=0\r\n");
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
