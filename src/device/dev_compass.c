/**
 * @file dev_compass.c
 * @brief 编译期所选罗盘的协议解析与校准命令。
 */
#include "dev_compass.h"
#include "bsp_power.h"
#include "bsp_uart.h"
#include "FreeRTOS.h"
#include "task.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define COMPASS_UART BSP_UART_COMPASS
#define RX_CAPACITY 128U

static compass_data_t s_data;
static compass_cal_state_t s_cal;
static uint8_t s_rx[RX_CAPACITY];
static size_t s_rx_len;
static volatile bool s_powered;
static TickType_t s_ready_tick;
static bool s_settling; /* 就绪后锁存，避免长期运行跨半个 Tick 周期被误判为未上电。 */

/* 命令时序由按键任务逐步推进，等待期间各任务仍能按期打卡。 */
typedef struct
{
    uint8_t bytes[12];
    uint8_t len;
    uint16_t wait_ms;
} command_step_t;

#if COMPASS_MODEL == COMPASS_MODEL_JY901B
#define JY_CMD(reg, lo, hi, wait) {{0xFF, 0xAA, reg, lo, hi}, 5U, wait}
static const command_step_t s_setup[] = {
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(0x23, 1, 0, 200), JY_CMD(0, 0, 0, 100),
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(2, 8, 0, 200), JY_CMD(0, 0, 0, 100),
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(3, 5, 0, 200), JY_CMD(0, 0, 0, 100),
};
static const command_step_t s_reset[] = {
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(0, 1, 0, 1000),
};
static const command_step_t s_accel[] = {
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(1, 1, 0, 4000),
    JY_CMD(1, 0, 0, 100), JY_CMD(0, 0, 0, 100),
};
static const command_step_t s_angle[] = {
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(1, 8, 0, 3000), JY_CMD(0, 0, 0, 100),
};
static const command_step_t s_mag_start[] = {
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(1, 7, 0, 0),
};
static const command_step_t s_mag_end[] = {
    JY_CMD(0x69, 0x88, 0xB5, 200), JY_CMD(1, 0, 0, 100), JY_CMD(0, 0, 0, 100),
};
#else
#if COMPASS_MODEL == COMPASS_MODEL_MCP406
static const command_step_t s_setup[] = {
    {{0, 7, 6, 0x0A, 0x17, 0x6E, 0x90}, 7, 50},
    {{0, 5, 9, 0x6E, 0xDC}, 5, 50},
    {{0, 9, 3, 3, 5, 0x18, 0x19, 0xDF, 0xDE}, 9, 50},
    {{0, 5, 0x15, 0xBD, 0x61}, 5, 50},
};
static const command_step_t s_reset[] = {
    {{0, 5, 0x1D, 0x3C, 0x69}, 5, 100},
    {{0, 5, 0x24, 0x9B, 0x13}, 5, 100},
};
#else
static const command_step_t s_setup[] = {
    {{0xAA, 0x55, 9, 0, 7, 3, 0x17, 0x1F, 0xBD}, 9, 50},
    {{0xAA, 0x55, 11, 0, 3, 3, 1, 2, 3, 0x2B, 0x1C}, 11, 50},
    {{0xAA, 0x55, 7, 0, 0x0D, 0xF1, 0x89}, 7, 50},
};
static const command_step_t s_reset[] = {
    {{0xAA, 0x55, 7, 0, 0x14, 0x72, 0x91}, 7, 100},
};
#endif

#if COMPASS_MODEL == COMPASS_MODEL_MCP406
static const command_step_t s_mag_start[] = {
    {{0, 9, 0x0A, 0, 0, 0, 0x0A, 0xAF, 0x06}, 9, 0},
};
static const command_step_t s_mag_sample[] = {
    {{0, 5, 0x1F, 0x1C, 0x2B}, 5, 0},
};
static const command_step_t s_mag_stop[] = {
    {{0, 5, 0x0B, 0x4E, 0x9E}, 5, 0},
};
static const command_step_t s_mag_save[] = {
    {{0, 5, 9, 0x6E, 0xDC}, 5, 0},
};
#else
static const command_step_t s_mag_start[] = {
    {{0xAA, 0x55, 8, 0, 0x0F, 1, 0xD4, 0x93}, 8, 0},
};
static const command_step_t s_mag_sample[] = {
    {{0xAA, 0x55, 7, 0, 0x11, 0x22, 0x34}, 7, 0},
};
static const command_step_t s_mag_stop[] = {
    {{0xAA, 0x55, 7, 0, 0x10, 0x32, 0x15}, 7, 0},
};
#endif

static uint16_t crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0;
    size_t i;
    uint8_t bit;
    for (i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; ++bit)
        {
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
        }
    }
    return crc;
}

static float float_be(const uint8_t* data)
{
    uint32_t bits = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                    ((uint32_t)data[2] << 8) | data[3];
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
#endif

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
static const command_step_t* s_sequence;
static size_t s_sequence_count;
static size_t s_sequence_index;
static TickType_t s_sent_tick;
static TickType_t s_wait_ticks;
static bool s_reconfigure;

static void start_sequence(const command_step_t* steps, size_t count, bool reconfigure)
{
    taskENTER_CRITICAL();
    s_sequence = steps;
    s_sequence_count = count;
    s_sequence_index = 0;
    s_sent_tick = xTaskGetTickCount();
    s_wait_ticks = 0;
    s_reconfigure = reconfigure;
    taskEXIT_CRITICAL();
}

bool compass_is_busy(void)
{
    bool busy;
    taskENTER_CRITICAL();
    busy = s_sequence != NULL;
    taskEXIT_CRITICAL();
    return busy;
}

bool compass_is_ready(void)
{
    bool ready;
    taskENTER_CRITICAL();
    if (s_settling && (int32_t)(xTaskGetTickCount() - s_ready_tick) >= 0)
    {
        s_settling = false;
    }
    ready = s_powered && !s_settling;
    taskEXIT_CRITICAL();
    return ready;
}

void compass_step(void)
{
    const command_step_t* step;
    TickType_t now;

    vTaskSuspendAll();
    taskENTER_CRITICAL();
    now = xTaskGetTickCount();
    if (s_sequence == NULL || !compass_is_ready() ||
        (TickType_t)(now - s_sent_tick) < s_wait_ticks)
    {
        taskEXIT_CRITICAL();
        (void)xTaskResumeAll();
        return;
    }
    if (s_sequence_index == s_sequence_count)
    {
        if (s_reconfigure)
        {
            start_sequence(s_setup, ARRAY_COUNT(s_setup), false);
        }
        else
        {
            s_sequence = NULL;
            taskEXIT_CRITICAL();
            (void)xTaskResumeAll();
            return;
        }
    }
    step = &s_sequence[s_sequence_index++];
    taskEXIT_CRITICAL();
    /* 仅暂停任务切换，串口发送期间保持中断开启，避免丢失 GNSS 接收字节。
     * 供电与命令接口仅供任务调用，因此关机不会插入此段固定长度发送。 */
    bsp_uart_write(COMPASS_UART, step->bytes, step->len);
    s_sent_tick = xTaskGetTickCount();
    s_wait_ticks = pdMS_TO_TICKS(step->wait_ms);
    (void)xTaskResumeAll();
}

const char* compass_model_name(void)
{
#if COMPASS_MODEL == COMPASS_MODEL_JY901B
    return "JY901B";
#elif COMPASS_MODEL == COMPASS_MODEL_MCP406
    return "MCP406";
#else
    return "MCG505";
#endif
}

void compass_power_ctl(bool on)
{
    taskENTER_CRITICAL();
    if (on == s_powered)
    {
        taskEXIT_CRITICAL();
        return;
    }
    s_powered = on;
    s_rx_len = 0;
    memset(&s_data, 0, sizeof(s_data));
    s_sequence = NULL;
    s_ready_tick = on ? xTaskGetTickCount() + pdMS_TO_TICKS(500U) : 0;
    s_settling = on;
    if (on)
    {
        start_sequence(s_setup, ARRAY_COUNT(s_setup), false);
    }
    bsp_pwr_compass(on);
    bsp_uart_flush_rx(COMPASS_UART);
    taskEXIT_CRITICAL();
}

void compass_init(void)
{
#if COMPASS_MODEL == COMPASS_MODEL_JY901B
    bsp_uart_init(COMPASS_UART, 9600U);
#else
    bsp_uart_init(COMPASS_UART, 38400U);
#endif
    compass_power_ctl(true);
    memset(&s_cal, 0, sizeof(s_cal));
    s_cal.cal_score = -1.0f;
    /* 上电已排队配置命令；运行阶段通过 compass_step() 逐步完成。 */
    /* 启动自检在开启看门狗之前运行，使用相同的非阻塞序列推进。 */
    while (compass_is_busy())
    {
        compass_step();
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
    bsp_uart_flush_rx(COMPASS_UART);
}

static void publish_data(const compass_data_t* data)
{
    taskENTER_CRITICAL();
    s_data = *data;
    taskEXIT_CRITICAL();
}

#if COMPASS_MODEL != COMPASS_MODEL_JY901B
static void handle_packet(uint8_t command, const uint8_t* payload, size_t len)
{
#if COMPASS_MODEL == COMPASS_MODEL_MCP406
    const uint8_t data_cmd = 5, count_cmd = 0x11, score_cmd = 0x12;
    const uint8_t heading_id = 5, pitch_id = 24, roll_id = 25;
#else
    const uint8_t data_cmd = 6, count_cmd = 0x12, score_cmd = 0x13;
    const uint8_t heading_id = 1, pitch_id = 2, roll_id = 3;
#endif
    if (command == data_cmd)
    {
        compass_data_t next = {0};
        uint8_t seen = 0;
        size_t i;
        if (len < 1 || payload[0] != 3 || len != 16)
        {
            return;
        }
        for (i = 1; i + 5 <= len; i += 5)
        {
            float value = float_be(payload + i + 1);
            if (!isfinite(value))
            {
                return;
            }
            if (payload[i] == heading_id)
            {
                next.heading = fmodf(value, 360.0f);
                if (next.heading < 0) next.heading += 360.0f;
                seen |= 1;
            }
            else if (payload[i] == pitch_id)
            {
                next.pitch = fmaxf(-90.0f, fminf(90.0f, value));
                seen |= 2;
            }
            else if (payload[i] == roll_id)
            {
                next.roll = value;
                seen |= 4;
            }
        }
        if (seen == 7)
        {
            next.tick_angle = xTaskGetTickCount();
            publish_data(&next);
        }
    }
    else if (command == count_cmd)
    {
#if COMPASS_MODEL == COMPASS_MODEL_MCP406
        if (len == 4)
        {
            s_cal.sample_count = ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
                                 ((uint32_t)payload[2] << 8) | payload[3];
        }
#else
        if (len == 1) s_cal.sample_count = payload[0];
#endif
    }
    else if (command == score_cmd && len >= 4)
    {
        float value = float_be(payload);
        taskENTER_CRITICAL();
        s_cal.cal_score = value;
        s_cal.score_valid = isfinite(value);
        taskEXIT_CRITICAL();
    }
}
#endif

static void drop_rx(size_t count)
{
    memmove(s_rx, s_rx + count, s_rx_len - count);
    s_rx_len -= count;
}

void compass_poll(void)
{
    uint8_t byte;
    for (;;)
    {
        taskENTER_CRITICAL();
        if (!compass_is_ready() || bsp_uart_read(COMPASS_UART, &byte, 1) != 1)
        {
            taskEXIT_CRITICAL();
            return;
        }
        if (s_rx_len == sizeof(s_rx)) drop_rx(1);
        s_rx[s_rx_len++] = byte;
        while (s_rx_len != 0)
        {
            size_t len;
#if COMPASS_MODEL == COMPASS_MODEL_JY901B
            uint8_t sum = 0;
            size_t i;
            if (s_rx[0] != 0x55) { drop_rx(1); continue; }
            if (s_rx_len < 11) break;
            len = 11;
            for (i = 0; i < 10; ++i) sum = (uint8_t)(sum + s_rx[i]);
            if (sum != s_rx[10]) { drop_rx(1); continue; }
            if (s_rx[1] == 0x53)
            {
                compass_data_t next;
                next.roll = (float)(int16_t)((uint16_t)s_rx[2] | ((uint16_t)s_rx[3] << 8)) * (180.0f / 32768.0f);
                /* 安装方向的符号换算只在此处执行，姿态应用层不再重复取反。 */
                next.pitch = -(float)(int16_t)((uint16_t)s_rx[4] | ((uint16_t)s_rx[5] << 8)) * (180.0f / 32768.0f);
                next.heading = -(float)(int16_t)((uint16_t)s_rx[6] | ((uint16_t)s_rx[7] << 8)) * (180.0f / 32768.0f);
                if (next.heading < 0) next.heading += 360.0f;
                next.tick_angle = xTaskGetTickCount();
                publish_data(&next);
            }
#else
#if COMPASS_MODEL == COMPASS_MODEL_MCG505
            if (s_rx[0] != 0xAA) { drop_rx(1); continue; }
            if (s_rx_len < 2) break;
            if (s_rx[1] != 0x55) { drop_rx(1); continue; }
            if (s_rx_len < 3) break;
            len = s_rx[2];
            if (len < 7 || len > sizeof(s_rx)) { drop_rx(1); continue; }
#else
            if (s_rx_len < 2) break;
            len = ((size_t)s_rx[0] << 8) | s_rx[1];
            if (len < 5 || len > sizeof(s_rx)) { drop_rx(1); continue; }
#endif
            if (s_rx_len < len)
            {
#if COMPASS_MODEL == COMPASS_MODEL_MCP406
                /* 仅靠长度定界可能在坏帧后误锁定数据区。
                 * 若后方存在完整且 CRC 正确的帧，优先重新同步到该帧。 */
                size_t offset;
                bool found = false;
                for (offset = 1; offset + 5 <= s_rx_len; ++offset)
                {
                    size_t candidate = ((size_t)s_rx[offset] << 8) | s_rx[offset + 1];
                    if (candidate >= 5 && candidate <= s_rx_len - offset &&
                        crc16(s_rx + offset, candidate - 2) ==
                        (uint16_t)(((uint16_t)s_rx[offset + candidate - 2] << 8) |
                                   s_rx[offset + candidate - 1]))
                    {
                        drop_rx(offset);
                        found = true;
                        break;
                    }
                }
                if (found) continue;
#endif
                break;
            }
            if (crc16(s_rx, len - 2) != (uint16_t)(((uint16_t)s_rx[len - 2] << 8) | s_rx[len - 1]))
            {
                drop_rx(1);
                continue;
            }
#if COMPASS_MODEL == COMPASS_MODEL_MCG505
            if (s_rx[3] == 0) handle_packet(s_rx[4], s_rx + 5, len - 7);
#else
            handle_packet(s_rx[2], s_rx + 3, len - 5);
#endif
#endif
            drop_rx(len);
        }
        taskEXIT_CRITICAL();
    }
}

const compass_data_t* compass_get_data(void) { return &s_data; }
const compass_cal_state_t* compass_get_cal_state(void) { return &s_cal; }

void compass_get_data_snapshot(compass_data_t* out)
{
    if (out == NULL) return;
    taskENTER_CRITICAL();
    *out = s_data;
    taskEXIT_CRITICAL();
}

void compass_get_cal_state_snapshot(compass_cal_state_t* out)
{
    if (out == NULL) return;
    taskENTER_CRITICAL();
    *out = s_cal;
    taskEXIT_CRITICAL();
}

bool compass_is_alive(uint32_t timeout_ms)
{
    compass_data_t data;
    bool powered;
    taskENTER_CRITICAL();
    powered = s_powered;
    data = s_data;
    taskEXIT_CRITICAL();
    return powered && data.tick_angle != 0 &&
           (TickType_t)(xTaskGetTickCount() - data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

bool compass_self_check(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    while ((TickType_t)(xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        compass_poll();
        if (compass_is_alive(timeout_ms)) return true;
        vTaskDelay(pdMS_TO_TICKS(20U));
    }
    return false;
}

bool compass_mag_uses_samples(void)
{
    return COMPASS_MODEL != COMPASS_MODEL_JY901B;
}

bool compass_calib_score_valid(float score)
{
#if COMPASS_MODEL == COMPASS_MODEL_MCP406
    return isfinite(score) && score > 0.0f && score <= 1.02f;
#elif COMPASS_MODEL == COMPASS_MODEL_MCG505
    return isfinite(score) && score > 0.0f && score <= 0.36f;
#else
    (void)score;
    return false;
#endif
}

void compass_calib_mag_start(void)
{
    bool powered;
    taskENTER_CRITICAL();
    powered = s_powered;
    s_cal.sample_count = compass_mag_uses_samples() ? 1U : 0U;
    s_cal.cal_score = -1.0f;
    s_cal.score_valid = false;
    taskEXIT_CRITICAL();
    if (powered && !compass_is_busy())
    {
        start_sequence(s_mag_start, ARRAY_COUNT(s_mag_start), false);
    }
}

void compass_calib_take_sample(void)
{
#if COMPASS_MODEL != COMPASS_MODEL_JY901B
    compass_cal_state_t cal;
    bool powered;
    taskENTER_CRITICAL();
    powered = s_powered;
    cal = s_cal;
    taskEXIT_CRITICAL();
    if (powered && !compass_is_busy() && !cal.score_valid)
    {
#if COMPASS_MODEL == COMPASS_MODEL_MCP406
        start_sequence(s_mag_sample, ARRAY_COUNT(s_mag_sample), false);
#else
        start_sequence(s_mag_sample, ARRAY_COUNT(s_mag_sample), false);
#endif
    }
#endif
}

void compass_calib_mag_end(void)
{
    bool powered;
#if COMPASS_MODEL != COMPASS_MODEL_JY901B
    compass_cal_state_t cal;
#endif
    taskENTER_CRITICAL();
    powered = s_powered;
#if COMPASS_MODEL != COMPASS_MODEL_JY901B
    cal = s_cal;
#endif
    taskEXIT_CRITICAL();
    if (!powered || compass_is_busy()) return;
#if COMPASS_MODEL == COMPASS_MODEL_JY901B
    start_sequence(s_mag_end, ARRAY_COUNT(s_mag_end), false);
#elif COMPASS_MODEL == COMPASS_MODEL_MCP406
    if (!cal.score_valid)
    {
        start_sequence(s_mag_stop, ARRAY_COUNT(s_mag_stop), false);
    }
    else if (compass_calib_score_valid(cal.cal_score))
    {
        start_sequence(s_mag_save, ARRAY_COUNT(s_mag_save), false);
    }
#else
    if (!cal.score_valid)
    {
        start_sequence(s_mag_stop, ARRAY_COUNT(s_mag_stop), false);
    }
#endif
}

bool compass_factory_reset(void)
{
    if (!s_powered || compass_is_busy()) return false;
    start_sequence(s_reset, ARRAY_COUNT(s_reset), true);
    return true;
}

bool compass_calib_accel(void)
{
#if COMPASS_MODEL == COMPASS_MODEL_JY901B
    if (!s_powered || compass_is_busy()) return false;
    start_sequence(s_accel, ARRAY_COUNT(s_accel), false);
    return true;
#else
    return false;
#endif
}

bool compass_calib_angle_ref(void)
{
#if COMPASS_MODEL == COMPASS_MODEL_JY901B
    if (!s_powered || compass_is_busy()) return false;
    start_sequence(s_angle, ARRAY_COUNT(s_angle), false);
    return true;
#else
    return false;
#endif
}
