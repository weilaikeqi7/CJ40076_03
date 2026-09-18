/**
 * @file dev_compass.c
 * @brief JY901B 姿态传感器设备驱动实现
 */
#include "dev_compass.h"

#include "bsp_power.h"
#include "bsp_uart.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define JY901B_UART BSP_UART_COMPASS
#define JY901B_BAUD 9600U
#define JY901B_FRAME_LEN 11U

#define JY901B_TYPE_ANGLE 0x53U
#define JY901B_REG_SAVE   0x00U
#define JY901B_REG_CALSW  0x01U
#define JY901B_REG_RSW    0x02U
#define JY901B_REG_RRATE  0x03U
#define JY901B_REG_ORIENT 0x23U
#define JY901B_REG_KEY    0x69U
#define JY901B_UNLOCK_KEY 0xB588U
#define JY901B_RSW_ANGLE  0x0008U
#define JY901B_RATE_5HZ   0x0005U

static jy901b_data_t s_data;

static int16_t frame_i16(const uint8_t* p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static void write_reg_raw(uint8_t reg, uint16_t value)
{
    const uint8_t cmd[5] = {0xFFU, 0xAAU, reg, (uint8_t)value, (uint8_t)(value >> 8U)};
    bsp_uart_write(JY901B_UART, cmd, sizeof(cmd));
}

static void unlock(void)
{
    write_reg_raw(JY901B_REG_KEY, JY901B_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
}

static void write_reg_save(uint8_t reg, uint16_t value)
{
    unlock();
    write_reg_raw(reg, value);
    vTaskDelay(pdMS_TO_TICKS(200U));
    write_reg_raw(JY901B_REG_SAVE, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

static void handle_angle_frame(const uint8_t* data)
{
    uint32_t now = xTaskGetTickCount();
    float yaw = -(float)frame_i16(&data[4]) / 32768.0f * 180.0f;

    s_data.roll = (float)frame_i16(&data[0]) / 32768.0f * 180.0f;
    s_data.pitch = -(float)frame_i16(&data[2]) / 32768.0f * 180.0f;
    while (yaw < 0.0f) yaw += 360.0f;
    while (yaw >= 360.0f) yaw -= 360.0f;
    s_data.heading = yaw;
    s_data.tick_angle = now;
}

void jy901b_poll(void)
{
    static uint8_t frame[JY901B_FRAME_LEN];
    static uint8_t index;
    uint8_t byte;
    uint8_t sum;
    uint8_t i;

    while (bsp_uart_read(JY901B_UART, &byte, 1U) == 1U)
    {
        if (index == 0U)
        {
            if (byte == 0x55U) frame[index++] = byte;
            continue;
        }
        frame[index++] = byte;
        if (index < JY901B_FRAME_LEN) continue;

        index = 0U;
        sum = 0U;
        for (i = 0U; i < JY901B_FRAME_LEN - 1U; i++) sum = (uint8_t)(sum + frame[i]);
        if (sum == frame[JY901B_FRAME_LEN - 1U] && frame[1] == JY901B_TYPE_ANGLE)
        {
            handle_angle_frame(&frame[2]);
        }
    }
}

void jy901b_power_ctl(bool on)
{
    bsp_pwr_compass(on);
    if (on)
    {
        vTaskDelay(pdMS_TO_TICKS(300U));
        bsp_uart_flush_rx(JY901B_UART);
    }
}

void jy901b_init(void)
{
    jy901b_power_ctl(true);
    bsp_uart_init(JY901B_UART, JY901B_BAUD);
    vTaskDelay(pdMS_TO_TICKS(500U));
    bsp_uart_flush_rx(JY901B_UART);

    write_reg_save(JY901B_REG_ORIENT, 0x0001U);
    write_reg_save(JY901B_REG_RSW, JY901B_RSW_ANGLE);
    write_reg_save(JY901B_REG_RRATE, JY901B_RATE_5HZ);

    memset(&s_data, 0, sizeof(s_data));
}

bool jy901b_self_check(uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        jy901b_poll();
        if (s_data.tick_angle != 0U) return true;
        vTaskDelay(pdMS_TO_TICKS(20U));
    }
    return false;
}

const jy901b_data_t* jy901b_get_data(void)
{
    return &s_data;
}

bool jy901b_is_alive(uint32_t timeout_ms)
{
    return s_data.tick_angle != 0U &&
           (xTaskGetTickCount() - s_data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

void jy901b_calib_mag_start(void)
{
    unlock();
    write_reg_raw(JY901B_REG_CALSW, 0x0007U);
    LOGI("jy901b: magnetic calibration started; rotate all three axes\r\n");
}

void jy901b_calib_mag_end(void)
{
    unlock();
    write_reg_raw(JY901B_REG_CALSW, 0x0000U);
    vTaskDelay(pdMS_TO_TICKS(100U));
    write_reg_raw(JY901B_REG_SAVE, 0x0000U);
    vTaskDelay(pdMS_TO_TICKS(100U));
    LOGI("jy901b: magnetic calibration ended and saved\r\n");
}

void jy901b_calib_accel(void)
{
    unlock();
    write_reg_raw(JY901B_REG_CALSW, 0x0001U);
    vTaskDelay(pdMS_TO_TICKS(4000U));
    write_reg_raw(JY901B_REG_CALSW, 0x0000U);
    vTaskDelay(pdMS_TO_TICKS(100U));
    write_reg_raw(JY901B_REG_SAVE, 0x0000U);
}

void jy901b_calib_angle_ref(void)
{
    unlock();
    write_reg_raw(JY901B_REG_CALSW, 0x0008U);
    vTaskDelay(pdMS_TO_TICKS(3000U));
    write_reg_raw(JY901B_REG_SAVE, 0x0000U);
}

void jy901b_calib_yaw_zero(void)
{
    unlock();
    write_reg_raw(JY901B_REG_CALSW, 0x0004U);
    vTaskDelay(pdMS_TO_TICKS(3000U));
    write_reg_raw(JY901B_REG_SAVE, 0x0000U);
}

void jy901b_factory_reset(void)
{
    unlock();
    write_reg_raw(JY901B_REG_SAVE, 0x0001U);
    vTaskDelay(pdMS_TO_TICKS(1000U));
    bsp_uart_flush_rx(JY901B_UART);
    write_reg_save(JY901B_REG_ORIENT, 0x0001U);
    write_reg_save(JY901B_REG_RSW, JY901B_RSW_ANGLE);
    write_reg_save(JY901B_REG_RRATE, JY901B_RATE_5HZ);
}
