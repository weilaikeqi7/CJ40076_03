/**
 * @file mcp406.c
 * @brief MCP-406-TTL 三维高精度电子罗盘驱动实现（USART2 38400 8N1）
 */
#include "mcp406.h"

#include "board.h"
#include "board_uart.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define MCP406_UART       BOARD_UART_JY901B
#define MCP406_FRAME_MAX  128U

static mcp406_data_t      s_mcp406_data;
static mcp406_cal_state_t s_cal_state;

/* ----------------------------- CRC-16 校验 ----------------------------- */

uint16_t mcp406_crc16(const uint8_t* buffer, uint16_t len)
{
    uint32_t crc = 0U;
    uint16_t i;

    for (i = 0U; i < len; i++)
    {
        crc = ((crc >> 8U) | (crc << 8U)) & 0xFFFFU;
        crc ^= buffer[i];
        crc &= 0xFFFFU;
        crc ^= (crc & 0x00FFU) >> 4U;
        crc &= 0xFFFFU;
        crc ^= (crc << 12U) & 0xFFFFU;
        crc &= 0xFFFFU;
        crc ^= ((crc & 0x00FFU) << 5U) & 0xFFFFU;
        crc &= 0xFFFFU;
    }
    return (uint16_t)(crc & 0xFFFFU);
}

static float parse_float_be(const uint8_t* p)
{
    union
    {
        float   f;
        uint8_t b[4];
    } u;
    u.b[0] = p[3];
    u.b[1] = p[2];
    u.b[2] = p[1];
    u.b[3] = p[0];
    return u.f;
}

static void send_cmd(const uint8_t* cmd, size_t len)
{
    board_uart_write(MCP406_UART, cmd, len);
}

/* ----------------------------- 帧接收与解析 ----------------------------- */

static void handle_frame(uint8_t cmd_id, const uint8_t* payload, uint16_t payload_len)
{
    uint32_t now = xTaskGetTickCount();

    switch (cmd_id)
    {
    case 0x05: /* GetDataResp（10Hz 广播帧：包含 Heading/Pitch/Roll） */
        if (payload_len >= 16U && payload[0] == 3U)
        {
            uint16_t idx = 1U;
            while (idx + 5U <= payload_len)
            {
                uint8_t comp_id = payload[idx];
                float   val     = parse_float_be(&payload[idx + 1U]);
                idx += 5U;

                if (comp_id == 5U) /* 方位角 (Heading) */
                {
                    /* 限制在 0.00° ~ 359.99° */
                    if (val < 0.0f)
                    {
                        val += 360.0f;
                    }
                    if (val >= 360.0f)
                    {
                        val = 0.0f;
                    }
                    s_mcp406_data.heading = val;
                }
                else if (comp_id == 24U) /* 俯仰角 (Pitch) */
                {
                    if (val > 90.0f)
                    {
                        val = 90.0f;
                    }
                    if (val < -90.0f)
                    {
                        val = -90.0f;
                    }
                    s_mcp406_data.pitch = val;
                }
                else if (comp_id == 25U) /* 横滚角 (Roll) */
                {
                    s_mcp406_data.roll = val;
                }
            }
            s_mcp406_data.tick_angle = now;
        }
        break;

    case 0x11: /* UserCalSampCount（已采样点数返回，Uint32 大端） */
        if (payload_len >= 4U)
        {
            uint32_t count = ((uint32_t)payload[0] << 24U) | ((uint32_t)payload[1] << 16U) |
                             ((uint32_t)payload[2] << 8U) | (uint32_t)payload[3];
            s_cal_state.sample_count = count;
            LOGI("mcp406: sample count = %lu\r\n", (unsigned long)count);
        }
        break;

    case 0x12: /* CalScore（校准得分返回，Float32 大端） */
        if (payload_len >= 4U)
        {
            float score               = parse_float_be(&payload[0]);
            s_cal_state.cal_score     = score;
            s_cal_state.score_valid   = true;
            LOGI("mcp406: cal score received = %d.%02d\r\n", (int)score, (int)((score - (int)score) * 100));
        }
        break;

    default:
        break;
    }
}

void mcp406_poll(void)
{
    static uint8_t  rx_buf[MCP406_FRAME_MAX];
    static uint16_t rx_idx = 0U;
    uint8_t         byte;

    while (board_uart_read(MCP406_UART, &byte, 1U) == 1U)
    {
        rx_buf[rx_idx++] = byte;

        /* 至少收到 2 字节长度 */
        if (rx_idx >= 2U)
        {
            uint16_t frame_len = ((uint16_t)rx_buf[0] << 8U) | rx_buf[1];

            /* 长度异常或超出缓冲区：滑动丢弃首字节 */
            if (frame_len < 5U || frame_len > MCP406_FRAME_MAX)
            {
                uint16_t k;
                for (k = 1U; k < rx_idx; k++)
                {
                    rx_buf[k - 1U] = rx_buf[k];
                }
                rx_idx--;
                continue;
            }

            /* 收齐一完整帧 */
            if (rx_idx == frame_len)
            {
                uint16_t calc_crc = mcp406_crc16(rx_buf, (uint16_t)(frame_len - 2U));
                uint16_t rx_crc   = ((uint16_t)rx_buf[frame_len - 2U] << 8U) | rx_buf[frame_len - 1U];

                if (calc_crc == rx_crc)
                {
                    handle_frame(rx_buf[2], &rx_buf[3], (uint16_t)(frame_len - 5U));
                    rx_idx = 0U;
                }
                else
                {
                    /* CRC 不符：丢弃首字节重新同步 */
                    uint16_t k;
                    for (k = 1U; k < rx_idx; k++)
                    {
                        rx_buf[k - 1U] = rx_buf[k];
                    }
                    rx_idx--;
                }
            }
        }
    }
}

/* ----------------------------- 对外接口 ----------------------------- */

void mcp406_init(void)
{
    /* 预制指令报文 */
    static const uint8_t CMD_SET_MOUNT[]   = {0x00, 0x07, 0x06, 0x0A, 0x17, 0x6E, 0x90}; /* Y轴朝下180° */
    static const uint8_t CMD_SET_COMPS[]   = {0x00, 0x09, 0x03, 0x03, 0x05, 0x18, 0x19, 0xDF, 0xDE}; /* 方位/俯仰/横滚 */
    static const uint8_t CMD_START_CONT[]  = {0x00, 0x05, 0x15, 0xBD, 0x61}; /* 10Hz 广播输出 */
    static const uint8_t CMD_SAVE[]        = {0x00, 0x05, 0x09, 0x6E, 0xDC}; /* 保存至 EEPROM */

    board_jy901b_power(true);
    board_uart_init(MCP406_UART, 38400U); /* MCP-406 默认 38400 波特率 */

    vTaskDelay(pdMS_TO_TICKS(350U)); /* 等待罗盘上电初始化完成 */
    board_uart_flush_rx(MCP406_UART);

    /* 1. 设置安装方式为：Y 轴朝下 180° */
    send_cmd(CMD_SET_MOUNT, sizeof(CMD_SET_MOUNT));
    vTaskDelay(pdMS_TO_TICKS(50U));

    /* 2. 保存安装方式配置到 EEPROM */
    send_cmd(CMD_SAVE, sizeof(CMD_SAVE));
    vTaskDelay(pdMS_TO_TICKS(50U));

    /* 3. 设置输出数据组件为 方位角、俯仰角、横滚角 */
    send_cmd(CMD_SET_COMPS, sizeof(CMD_SET_COMPS));
    vTaskDelay(pdMS_TO_TICKS(50U));

    /* 4. 开启 10Hz 广播模式（下次上电自动广播） */
    send_cmd(CMD_START_CONT, sizeof(CMD_START_CONT));
    vTaskDelay(pdMS_TO_TICKS(50U));

    board_uart_flush_rx(MCP406_UART);
    memset(&s_mcp406_data, 0, sizeof(s_mcp406_data));
    memset(&s_cal_state, 0, sizeof(s_cal_state));
    s_cal_state.cal_score = -1.0f;
}

void mcp406_power_ctl(bool on)
{
    board_compass_power(on);
    if (on)
    {
        /* 重新上电后等待模块启动，刷新接收缓存 */
        vTaskDelay(pdMS_TO_TICKS(300U));
        board_uart_flush_rx(MCP406_UART);
    }
}

bool mcp406_self_check(uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        mcp406_poll();
        if (s_mcp406_data.tick_angle != 0U)
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20U));
    }
    return false;
}

const mcp406_data_t* mcp406_get_data(void)
{
    return &s_mcp406_data;
}

const mcp406_cal_state_t* mcp406_get_cal_state(void)
{
    return &s_cal_state;
}

bool mcp406_is_alive(uint32_t timeout_ms)
{
    if (s_mcp406_data.tick_angle == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - s_mcp406_data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

void mcp406_start_mag_cal(void)
{
    /* TCM 5 型 空间手动校准指令（0x0A，参数 10）：00 09 0A 00 00 00 0A AF 06 */
    static const uint8_t CMD_START_MAG_CAL[] = {0x00, 0x09, 0x0A, 0x00, 0x00, 0x00, 0x0A, 0xAF, 0x06};

    s_cal_state.sample_count = 1U; /* 手册明确说明：发送开始校准指令后自动采集第1组数据并返回编号1 */
    s_cal_state.cal_score    = -1.0f;
    s_cal_state.score_valid  = false;

    send_cmd(CMD_START_MAG_CAL, sizeof(CMD_START_MAG_CAL));
    LOGI("mcp406: start mag space manual calibration sent (init count=1)\r\n");
}

void mcp406_take_sample(void)
{
    /* 单次采样指令 TakeUserCalSample (0x1F): 00 05 1F 1C 2B */
    static const uint8_t CMD_TAKE_SAMPLE[] = {0x00, 0x05, 0x1F, 0x1C, 0x2B};

    send_cmd(CMD_TAKE_SAMPLE, sizeof(CMD_TAKE_SAMPLE));
    LOGI("mcp406: take sample command sent\r\n");
}

void mcp406_stop_cal(void)
{
    /* 停止校准指令 StopCal (0x0B): 00 05 0B 4E 9E */
    static const uint8_t CMD_STOP_CAL[] = {0x00, 0x05, 0x0B, 0x4E, 0x9E};

    send_cmd(CMD_STOP_CAL, sizeof(CMD_STOP_CAL));
    LOGI("mcp406: stop cal command sent\r\n");
}

void mcp406_save(void)
{
    /* 保存指令 Save (0x09): 00 05 09 6E DC */
    static const uint8_t CMD_SAVE[] = {0x00, 0x05, 0x09, 0x6E, 0xDC};

    send_cmd(CMD_SAVE, sizeof(CMD_SAVE));
    LOGI("mcp406: save command sent to EEPROM\r\n");
}

void mcp406_factory_reset(void)
{
    /* 恢复磁力计与加速度计出厂参数 */
    static const uint8_t CMD_RST_MAG[]   = {0x00, 0x05, 0x1D, 0x3C, 0x69};
    static const uint8_t CMD_RST_ACC[]   = {0x00, 0x05, 0x24, 0x9B, 0x13};
    static const uint8_t CMD_SET_MOUNT[] = {0x00, 0x07, 0x06, 0x0A, 0x17, 0x6E, 0x90}; /* Y轴朝下180° */
    static const uint8_t CMD_START_CONT[]= {0x00, 0x05, 0x15, 0xBD, 0x61}; /* 10Hz 广播 */
    static const uint8_t CMD_SAVE[]      = {0x00, 0x05, 0x09, 0x6E, 0xDC};

    send_cmd(CMD_RST_MAG, sizeof(CMD_RST_MAG));
    vTaskDelay(pdMS_TO_TICKS(100U));
    send_cmd(CMD_RST_ACC, sizeof(CMD_RST_ACC));
    vTaskDelay(pdMS_TO_TICKS(100U));
    send_cmd(CMD_SET_MOUNT, sizeof(CMD_SET_MOUNT));
    vTaskDelay(pdMS_TO_TICKS(50U));
    send_cmd(CMD_START_CONT, sizeof(CMD_START_CONT));
    vTaskDelay(pdMS_TO_TICKS(50U));
    send_cmd(CMD_SAVE, sizeof(CMD_SAVE));
    vTaskDelay(pdMS_TO_TICKS(50U));

    LOGI("mcp406: factory reset and re-configured\r\n");
}
