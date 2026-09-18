/**
 * @file dev_compass.c
 * @brief JY901B attitude sensor implementation behind the common compass API
 */
#include "dev_compass.h"

#include "bsp_power.h"
#include "bsp_uart.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define COMPASS_UART BSP_UART_COMPASS
#define COMPASS_BAUD 9600U
#define JY_FRAME_LEN 11U

#define JY_TYPE_ANGLE 0x53U
#define JY_REG_SAVE   0x00U
#define JY_REG_CALSW  0x01U
#define JY_REG_RSW    0x02U
#define JY_REG_RRATE  0x03U
#define JY_REG_ORIENT 0x23U
#define JY_REG_KEY    0x69U
#define JY_UNLOCK_KEY 0xB588U
#define JY_RSW_ANGLE  0x0008U
#define JY_RATE_5HZ   0x05U

static mcp406_data_t      s_data;
static mcp406_cal_state_t s_cal;

static int16_t frame_i16(const uint8_t* p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static void write_reg_raw(uint8_t reg, uint16_t value)
{
    uint8_t cmd[5] = {0xFFU, 0xAAU, reg, (uint8_t)value, (uint8_t)(value >> 8U)};
    bsp_uart_write(COMPASS_UART, cmd, sizeof(cmd));
}

static void write_reg_save(uint8_t reg, uint16_t value)
{
    write_reg_raw(JY_REG_KEY, JY_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(reg, value);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(JY_REG_SAVE, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

static void write_reg_unlocked(uint8_t reg, uint16_t value)
{
    write_reg_raw(JY_REG_KEY, JY_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(reg, value);
}

static void handle_frame(uint8_t type, const uint8_t* d)
{
    uint32_t now = xTaskGetTickCount();

    if (type == JY_TYPE_ANGLE)
    {
        s_data.roll = (float)frame_i16(&d[0]) / 32768.0f * 180.0f;
        s_data.pitch = -(float)frame_i16(&d[2]) / 32768.0f * 180.0f;
        s_data.heading = -(float)frame_i16(&d[4]) / 32768.0f * 180.0f;
        if (s_data.heading < 0.0f) s_data.heading += 360.0f;
        if (s_data.heading >= 360.0f) s_data.heading -= 360.0f;
        s_data.tick_angle = now;
    }
}

void mcp406_poll(void)
{
    static uint8_t frame[JY_FRAME_LEN];
    static uint8_t index;
    uint8_t byte;
    uint8_t sum;
    uint8_t i;

    while (bsp_uart_read(COMPASS_UART, &byte, 1U) == 1U)
    {
        if (index == 0U)
        {
            if (byte == 0x55U) frame[index++] = byte;
            continue;
        }
        frame[index++] = byte;
        if (index < JY_FRAME_LEN) continue;

        index = 0U;
        sum = 0U;
        for (i = 0U; i < JY_FRAME_LEN - 1U; i++) sum = (uint8_t)(sum + frame[i]);
        if (sum == frame[JY_FRAME_LEN - 1U]) handle_frame(frame[1], &frame[2]);
    }
}

void mcp406_power_ctl(bool on)
{
    bsp_pwr_compass(on);
    if (on)
    {
        vTaskDelay(pdMS_TO_TICKS(300U));
        bsp_uart_flush_rx(COMPASS_UART);
    }
}

void mcp406_init(void)
{
    mcp406_power_ctl(true);
    bsp_uart_init(COMPASS_UART, COMPASS_BAUD);
    vTaskDelay(pdMS_TO_TICKS(500U));
    bsp_uart_flush_rx(COMPASS_UART);
    write_reg_save(JY_REG_ORIENT, 0x0001U);
    write_reg_save(JY_REG_RSW, JY_RSW_ANGLE);
    write_reg_save(JY_REG_RRATE, JY_RATE_5HZ);
    memset(&s_data, 0, sizeof(s_data));
    memset(&s_cal, 0, sizeof(s_cal));
    s_cal.cal_score = -1.0f;
}

bool mcp406_self_check(uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        mcp406_poll();
        if (s_data.tick_angle != 0U) return true;
        vTaskDelay(pdMS_TO_TICKS(20U));
    }
    return false;
}

const mcp406_data_t* mcp406_get_data(void) { return &s_data; }
const mcp406_cal_state_t* mcp406_get_cal_state(void) { return &s_cal; }

bool mcp406_is_alive(uint32_t timeout_ms)
{
    return s_data.tick_angle != 0U &&
           (xTaskGetTickCount() - s_data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

void mcp406_start_mag_cal(void)
{
    write_reg_unlocked(JY_REG_CALSW, 0x0007U);
    s_cal.sample_count = 1U;
    s_cal.score_valid = false;
    s_cal.cal_score = -1.0f;
    LOGI("jy901b: magnetic calibration started\r\n");
}

void mcp406_take_sample(void) { }

void mcp406_stop_cal(void)
{
    write_reg_raw(JY_REG_KEY, JY_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(JY_REG_CALSW, 0U);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(JY_REG_SAVE, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

void mcp406_save(void) { }

void mcp406_factory_reset(void)
{
    write_reg_raw(JY_REG_KEY, JY_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(JY_REG_SAVE, 1U);
    vTaskDelay(pdMS_TO_TICKS(1000U));
    bsp_uart_flush_rx(COMPASS_UART);
    write_reg_save(JY_REG_ORIENT, 0x0001U);
    write_reg_save(JY_REG_RSW, JY_RSW_ANGLE);
    write_reg_save(JY_REG_RRATE, JY_RATE_5HZ);
}

uint16_t mcp406_crc16(const uint8_t* buffer, uint16_t len)
{
    (void)buffer;
    (void)len;
    return 0U;
}
