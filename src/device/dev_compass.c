/**
 * @file dev_compass.c
 * @brief MCG505 型全国产三维高精度电子罗盘设备驱动实现（USART2 38400 8N1）
 *
 * 通讯协议：遵守 IEEE 标准 ANSI/IEEE Std 754-1985（大端单精度浮点数），CRC-CCITT (XModem) 校验。
 */
#include "dev_compass.h"

#include "bsp_power.h"
#include "bsp_uart.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define MCG505_UART      BSP_UART_COMPASS
#define MCG505_FRAME_MAX 128U

#define MCG505_HEAD0     0xAAU
#define MCG505_HEAD1     0x55U
#define MCG505_ADDR      0x00U

/* 协议命令标识符定义（参考用户手册表 4） */
#define CMD_GET_DATA_RESP         0x06U /* 测量数据响应 / 连续广播数据 */
#define CMD_SET_CONFIG_RESP       0x08U /* 参数设置响应 */
#define CMD_USER_CAL_SAMP_COUNT   0x12U /* 用户校准采样点数返回 */
#define CMD_CAL_SCORE             0x13U /* 用户校准得分返回 */

static mcg505_data_t      s_mcg505_data;
static mcg505_cal_state_t s_cal_state;

/* ----------------------------- CRC-16 校验 ----------------------------- */

/**
 * @brief CRC-16-CCITT (XModem) 校验（符合 MCG505 手册标准算法）
 */
uint16_t mcg505_crc16(const uint8_t* buffer, uint16_t len)
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

/**
 * @brief 解析 IEEE-754 4字节大端浮点数
 */
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
    bsp_uart_write(MCG505_UART, cmd, len);
}

/* ----------------------------- 帧接收与解析 ----------------------------- */

static void handle_frame(uint8_t cmd_id, const uint8_t* payload, uint16_t payload_len)
{
    uint32_t now = xTaskGetTickCount();

    switch (cmd_id)
    {
    case CMD_GET_DATA_RESP: /* 0x06: 10Hz 连续广播数据帧（包含 Heading/Pitch/Roll） */
        if (payload_len >= 6U)
        {
            uint8_t  count = payload[0];
            uint16_t idx   = 1U;
            (void)count;

            while (idx + 5U <= payload_len)
            {
                uint8_t comp_id = payload[idx];
                float   val     = parse_float_be(&payload[idx + 1U]);
                idx += 5U;

                if (comp_id == 1U) /* 方位角 (Heading，0.00° ~ 359.99°) */
                {
                    if (val < 0.0f)
                    {
                        val += 360.0f;
                    }
                    if (val >= 360.0f)
                    {
                        val = 0.0f;
                    }
                    s_mcg505_data.heading = val;
                }
                else if (comp_id == 2U) /* 俯仰角 (Pitch，-90.00° ~ +90.00°) */
                {
                    if (val > 90.0f)
                    {
                        val = 90.0f;
                    }
                    if (val < -90.0f)
                    {
                        val = -90.0f;
                    }
                    s_mcg505_data.pitch = val;
                }
                else if (comp_id == 3U) /* 横滚角 (Roll，-180.00° ~ +180.00°) */
                {
                    if (val > 180.0f)
                    {
                        val = 180.0f;
                    }
                    if (val < -180.0f)
                    {
                        val = -180.0f;
                    }
                    s_mcg505_data.roll = val;
                }
            }
            s_mcg505_data.tick_angle = now;
        }
        break;

    case CMD_USER_CAL_SAMP_COUNT: /* 0x12: 用户校准采样点数返回 (Count: Uint8) */
        if (payload_len >= 1U)
        {
            s_cal_state.sample_count = payload[0];
            LOGI("mcg505: sample count = %u\r\n", (unsigned int)payload[0]);
        }
        break;

    case CMD_CAL_SCORE: /* 0x13: 用户校准得分返回 (MagScore: Float32 大端) */
        if (payload_len >= 4U)
        {
            float score             = parse_float_be(&payload[0]);
            s_cal_state.cal_score   = score;
            s_cal_state.score_valid = true;
            LOGI("mcg505: cal score received = %d.%02d\r\n", (int)score,
                 (int)((score - (int)score) * 100));
        }
        break;

    default:
        break;
    }
}

void mcg505_poll(void)
{
    static uint8_t  rx_buf[MCG505_FRAME_MAX];
    static uint16_t rx_idx = 0U;
    uint8_t         byte;

    while (bsp_uart_read(MCG505_UART, &byte, 1U) == 1U)
    {
        /* 1. 寻找帧头 AA 55 */
        if (rx_idx == 0U)
        {
            if (byte == MCG505_HEAD0)
            {
                rx_buf[rx_idx++] = byte;
            }
            continue;
        }
        if (rx_idx == 1U)
        {
            if (byte == MCG505_HEAD1)
            {
                rx_buf[rx_idx++] = byte;
            }
            else
            {
                rx_idx = 0U;
                if (byte == MCG505_HEAD0)
                {
                    rx_buf[rx_idx++] = byte;
                }
            }
            continue;
        }

        rx_buf[rx_idx++] = byte;

        /* 2. 判断长度：第 3 字节 (rx_buf[2]) 为整帧总长度 */
        if (rx_idx >= 3U)
        {
            uint8_t frame_len = rx_buf[2];

            /* 长度异常判定：最小帧长 7 字节，最大不超过缓冲区 */
            if (frame_len < 7U || frame_len > MCG505_FRAME_MAX)
            {
                /* 丢弃首字节，在剩余字节中重新搜寻 AA 55 */
                uint16_t k;
                for (k = 1U; k < rx_idx; k++)
                {
                    rx_buf[k - 1U] = rx_buf[k];
                }
                rx_idx--;
                continue;
            }

            /* 3. 收齐整帧后执行校验 */
            if (rx_idx == frame_len)
            {
                /* 地址码必须为 0x00 */
                if (rx_buf[3] == MCG505_ADDR)
                {
                    uint16_t calc_crc = mcg505_crc16(rx_buf, (uint16_t)(frame_len - 2U));
                    uint16_t rx_crc   = ((uint16_t)rx_buf[frame_len - 2U] << 8U) | rx_buf[frame_len - 1U];

                    if (calc_crc == rx_crc)
                    {
                        /* 提取：标识符位于 rx_buf[4]，数据区位于 rx_buf[5]，长度为 frame_len - 7 */
                        handle_frame(rx_buf[4], &rx_buf[5], (uint16_t)(frame_len - 7U));
                        rx_idx = 0U;
                        continue;
                    }
                }

                /* 校验失败：丢弃首字节并向后滑动重试 */
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

/* ----------------------------- 对外接口 ----------------------------- */

void mcg505_init(void)
{
    /*
     * 预制报文（CRC-16 预先精确校验通过）：
     * 1. 设置安装方式为 Y轴朝下180° (Flag=3, Val=23=0x17): AA 55 09 00 07 03 17 1F BD
     * 2. 设置输出数据组件为 方位(1)/俯仰(2)/横滚(3):       AA 55 0B 00 03 03 01 02 03 2B 1C
     * 3. 开启 10Hz 连续广播输出:                         AA 55 07 00 0D F1 89
     */
    static const uint8_t CMD_SET_MOUNT[]  = {0xAA, 0x55, 0x09, 0x00, 0x07, 0x03, 0x17, 0x1F, 0xBD};
    static const uint8_t CMD_SET_COMPS[]  = {0xAA, 0x55, 0x0B, 0x00, 0x03, 0x03, 0x01, 0x02, 0x03, 0x2B, 0x1C};
    static const uint8_t CMD_START_CONT[] = {0xAA, 0x55, 0x07, 0x00, 0x0D, 0xF1, 0x89};

    bsp_pwr_compass(true);
    bsp_uart_init(MCG505_UART, 38400U); /* MCG505 出厂默认 38400 波特率 */

    vTaskDelay(pdMS_TO_TICKS(350U)); /* 等待罗盘上电初始化稳定 */
    bsp_uart_flush_rx(MCG505_UART);

    /* 1. 设置机械安装方式为：Y 轴朝下 180° */
    send_cmd(CMD_SET_MOUNT, sizeof(CMD_SET_MOUNT));
    vTaskDelay(pdMS_TO_TICKS(50U));

    /* 2. 设置输出数据组件为 方位角、俯仰角、横滚角 */
    send_cmd(CMD_SET_COMPS, sizeof(CMD_SET_COMPS));
    vTaskDelay(pdMS_TO_TICKS(50U));

    /* 3. 开启 10Hz 广播模式（内部自动持久化，下次上电自动广播） */
    send_cmd(CMD_START_CONT, sizeof(CMD_START_CONT));
    vTaskDelay(pdMS_TO_TICKS(50U));

    bsp_uart_flush_rx(MCG505_UART);
    memset(&s_mcg505_data, 0, sizeof(s_mcg505_data));
    memset(&s_cal_state, 0, sizeof(s_cal_state));
    s_cal_state.cal_score = -1.0f;
}

void mcg505_power_ctl(bool on)
{
    bsp_pwr_compass(on);
    if (on)
    {
        /* 重新上电后等待模块启动，刷新接收缓存 */
        vTaskDelay(pdMS_TO_TICKS(300U));
        bsp_uart_flush_rx(MCG505_UART);
    }
}

bool mcg505_self_check(uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        mcg505_poll();
        if (s_mcg505_data.tick_angle != 0U)
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20U));
    }
    return false;
}

const mcg505_data_t* mcg505_get_data(void)
{
    return &s_mcg505_data;
}

const mcg505_cal_state_t* mcg505_get_cal_state(void)
{
    return &s_cal_state;
}

bool mcg505_is_alive(uint32_t timeout_ms)
{
    if (s_mcg505_data.tick_angle == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - s_mcg505_data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

void mcg505_start_mag_cal(void)
{
    /* 开始磁场空间手动校准（0x0F, 模式 1: 空间手动校准）：AA 55 08 00 0F 01 D4 93 */
    static const uint8_t CMD_START_MAG_CAL[] = {0xAA, 0x55, 0x08, 0x00, 0x0F, 0x01, 0xD4, 0x93};

    s_cal_state.sample_count = 1U; /* 手册明确说明：发送开始校准指令后自动采集第 1 组数据并返回编号 1 */
    s_cal_state.cal_score    = -1.0f;
    s_cal_state.score_valid  = false;

    send_cmd(CMD_START_MAG_CAL, sizeof(CMD_START_MAG_CAL));
    LOGI("mcg505: start mag space manual calibration sent (init count=1)\r\n");
}

void mcg505_take_sample(void)
{
    /* 单次采样指令 TakeUserCalSample (0x11): AA 55 07 00 11 22 34 */
    static const uint8_t CMD_TAKE_SAMPLE[] = {0xAA, 0x55, 0x07, 0x00, 0x11, 0x22, 0x34};

    send_cmd(CMD_TAKE_SAMPLE, sizeof(CMD_TAKE_SAMPLE));
    LOGI("mcg505: take sample command sent\r\n");
}

void mcg505_stop_cal(void)
{
    /* 停止校准指令 StopCal (0x10): AA 55 07 00 10 32 15 */
    static const uint8_t CMD_STOP_CAL[] = {0xAA, 0x55, 0x07, 0x00, 0x10, 0x32, 0x15};

    send_cmd(CMD_STOP_CAL, sizeof(CMD_STOP_CAL));
    LOGI("mcg505: stop cal command sent\r\n");
}

void mcg505_factory_reset(void)
{
    /* 恢复出厂校准参数 FactoryRest (0x14): AA 55 07 00 14 72 91 */
    static const uint8_t CMD_RST_CAL[]    = {0xAA, 0x55, 0x07, 0x00, 0x14, 0x72, 0x91};
    static const uint8_t CMD_SET_MOUNT[]  = {0xAA, 0x55, 0x09, 0x00, 0x07, 0x03, 0x17, 0x1F, 0xBD}; /* Y轴朝下180° */
    static const uint8_t CMD_SET_COMPS[]  = {0xAA, 0x55, 0x0B, 0x00, 0x03, 0x03, 0x01, 0x02, 0x03, 0x2B, 0x1C};
    static const uint8_t CMD_START_CONT[] = {0xAA, 0x55, 0x07, 0x00, 0x0D, 0xF1, 0x89};             /* 10Hz 广播 */

    send_cmd(CMD_RST_CAL, sizeof(CMD_RST_CAL));
    vTaskDelay(pdMS_TO_TICKS(100U));
    send_cmd(CMD_SET_MOUNT, sizeof(CMD_SET_MOUNT));
    vTaskDelay(pdMS_TO_TICKS(50U));
    send_cmd(CMD_SET_COMPS, sizeof(CMD_SET_COMPS));
    vTaskDelay(pdMS_TO_TICKS(50U));
    send_cmd(CMD_START_CONT, sizeof(CMD_START_CONT));
    vTaskDelay(pdMS_TO_TICKS(50U));

    LOGI("mcg505: factory reset and re-configured\r\n");
}
