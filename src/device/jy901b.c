/**
 * @file jy901b.c
 * @brief JY901B 姿态传感器驱动实现
 */
#include "jy901b.h"

#include "board.h"
#include "board_uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#define JY901B_UART BOARD_UART_JY901B
#define JY901B_BAUD 9600U

/* 数据帧类型 */
#define JY901B_TYPE_TIME 0x50U
#define JY901B_TYPE_ACC  0x51U
#define JY901B_TYPE_GYRO 0x52U
#define JY901B_TYPE_ANGLE 0x53U
#define JY901B_TYPE_MAG  0x54U

/* 寄存器地址 */
#define JY901B_REG_SAVE   0x00U
#define JY901B_REG_CALSW  0x01U
#define JY901B_REG_RSW    0x02U
#define JY901B_REG_RRATE  0x03U
#define JY901B_REG_ORIENT 0x23U
#define JY901B_REG_KEY    0x69U

#define JY901B_UNLOCK_KEY 0xB588U

/* 帧长：0x55 + TYPE + 8 数据 + SUM */
#define JY901B_FRAME_LEN 11U

static jy901b_data_t jy901b_data;

/* ------------------------------ 协议收发 ------------------------------ */

static void jy901b_write_reg_raw(uint8_t reg, uint16_t value)
{
    uint8_t cmd[5];

    cmd[0] = 0xFFU;
    cmd[1] = 0xAAU;
    cmd[2] = reg;
    cmd[3] = (uint8_t)(value & 0xFFU);
    cmd[4] = (uint8_t)(value >> 8);
    board_uart_write(JY901B_UART, cmd, sizeof(cmd));
}

/** 解锁 -> 写寄存器 -> 保存（写操作流程，寄存器掉电保存） */
static void jy901b_write_reg_save(uint8_t reg, uint16_t value)
{
    jy901b_write_reg_raw(JY901B_REG_KEY, JY901B_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    jy901b_write_reg_raw(reg, value);
    vTaskDelay(pdMS_TO_TICKS(200U));
    jy901b_write_reg_raw(JY901B_REG_SAVE, 0x0000U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

/** 解锁 -> 写寄存器（不保存，用于校准类流程中间步骤） */
static void jy901b_write_reg_unlocked(uint8_t reg, uint16_t value)
{
    jy901b_write_reg_raw(JY901B_REG_KEY, JY901B_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    jy901b_write_reg_raw(reg, value);
}

/* ------------------------------ 帧解析 ------------------------------ */

static int16_t frame_i16(const uint8_t* p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void jy901b_handle_frame(uint8_t type, const uint8_t* d)
{
    uint32_t now = xTaskGetTickCount();

    switch (type)
    {
    case JY901B_TYPE_ACC:
        jy901b_data.acc_x  = (float)frame_i16(&d[0]) / 32768.0f * 16.0f;
        jy901b_data.acc_y  = (float)frame_i16(&d[2]) / 32768.0f * 16.0f;
        jy901b_data.acc_z  = (float)frame_i16(&d[4]) / 32768.0f * 16.0f;
        jy901b_data.temp_c = (float)frame_i16(&d[6]) / 100.0f;
        jy901b_data.tick_acc = now;
        break;

    case JY901B_TYPE_GYRO:
        jy901b_data.gyro_x = (float)frame_i16(&d[0]) / 32768.0f * 2000.0f;
        jy901b_data.gyro_y = (float)frame_i16(&d[2]) / 32768.0f * 2000.0f;
        jy901b_data.gyro_z = (float)frame_i16(&d[4]) / 32768.0f * 2000.0f;
        jy901b_data.tick_gyro = now;
        break;

    case JY901B_TYPE_ANGLE:
        jy901b_data.roll    = (float)frame_i16(&d[0]) / 32768.0f * 180.0f;
        jy901b_data.pitch   = (float)frame_i16(&d[2]) / 32768.0f * 180.0f;
        jy901b_data.yaw     = (float)frame_i16(&d[4]) / 32768.0f * 180.0f;
        jy901b_data.version = (uint16_t)frame_i16(&d[6]);
        jy901b_data.tick_angle = now;
        break;

    case JY901B_TYPE_MAG:
        jy901b_data.mag_x  = frame_i16(&d[0]);
        jy901b_data.mag_y  = frame_i16(&d[2]);
        jy901b_data.mag_z  = frame_i16(&d[4]);
        jy901b_data.temp_c = (float)frame_i16(&d[6]) / 100.0f;
        jy901b_data.tick_mag = now;
        break;

    default:
        break;
    }
}

void jy901b_poll(void)
{
    static uint8_t frame[JY901B_FRAME_LEN];
    static uint8_t index = 0U;
    uint8_t        byte;
    uint8_t        sum;
    uint8_t        i;

    while (board_uart_read(JY901B_UART, &byte, 1U) == 1U)
    {
        /* 帧头同步 */
        if (index == 0U)
        {
            if (byte == 0x55U)
            {
                frame[index++] = byte;
            }
            continue;
        }

        frame[index++] = byte;

        if (index < JY901B_FRAME_LEN)
        {
            continue;
        }

        /* 收满一帧，校验 */
        index = 0U;
        sum   = 0U;
        for (i = 0U; i < JY901B_FRAME_LEN - 1U; i++)
        {
            sum = (uint8_t)(sum + frame[i]);
        }
        if (sum == frame[JY901B_FRAME_LEN - 1U])
        {
            jy901b_handle_frame(frame[1], &frame[2]);
        }
    }
}

/* ------------------------------ 对外接口 ------------------------------ */

void jy901b_init(jy901b_rate_t rate, uint16_t content)
{
    board_jy901b_power(true);
    board_uart_init(JY901B_UART, JY901B_BAUD);

    /* 模块上电启动时间 */
    vTaskDelay(pdMS_TO_TICKS(500U));
    board_uart_flush_rx(JY901B_UART);

    /* 垂直安装（Y 轴箭头朝上） */
    jy901b_write_reg_save(JY901B_REG_ORIENT, 0x0001U);

    /* 输出内容与速率 */
    jy901b_write_reg_save(JY901B_REG_RSW, content);
    jy901b_write_reg_save(JY901B_REG_RRATE, (uint16_t)rate);

    memset(&jy901b_data, 0, sizeof(jy901b_data));
}

const jy901b_data_t* jy901b_get_data(void)
{
    return &jy901b_data;
}

bool jy901b_is_alive(uint32_t timeout_ms)
{
    if (jy901b_data.tick_angle == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - jy901b_data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

void jy901b_set_orient_vertical(bool vertical)
{
    jy901b_write_reg_save(JY901B_REG_ORIENT, vertical ? 0x0001U : 0x0000U);
}

void jy901b_calib_accel(void)
{
    jy901b_write_reg_unlocked(JY901B_REG_CALSW, 0x0001U);
    vTaskDelay(pdMS_TO_TICKS(4000U));
    jy901b_write_reg_raw(JY901B_REG_CALSW, 0x0000U); /* 退出校准 */
    vTaskDelay(pdMS_TO_TICKS(100U));
    jy901b_write_reg_save(JY901B_REG_SAVE, 0x0000U);
}

void jy901b_set_angle_ref(void)
{
    jy901b_write_reg_unlocked(JY901B_REG_CALSW, 0x0008U);
    vTaskDelay(pdMS_TO_TICKS(3000U));
    jy901b_write_reg_save(JY901B_REG_SAVE, 0x0000U);
}

void jy901b_yaw_zero(void)
{
    jy901b_write_reg_unlocked(JY901B_REG_CALSW, 0x0004U);
    vTaskDelay(pdMS_TO_TICKS(3000U));
    jy901b_write_reg_save(JY901B_REG_SAVE, 0x0000U);
}

void jy901b_factory_reset(void)
{
    /* 解锁 -> 恢复出厂（SAVE=0x0001）-> 等待模块重启 -> 重新配置本项目参数 */
    jy901b_write_reg_raw(JY901B_REG_KEY, JY901B_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    jy901b_write_reg_raw(JY901B_REG_SAVE, 0x0001U);
    vTaskDelay(pdMS_TO_TICKS(1000U));
    board_uart_flush_rx(JY901B_UART);

    jy901b_write_reg_save(JY901B_REG_ORIENT, 0x0001U);
    jy901b_write_reg_save(JY901B_REG_RSW, JY901B_RSW_ANGLE);
    jy901b_write_reg_save(JY901B_REG_RRATE, (uint16_t)JY901B_RATE_5HZ);
}

void jy901b_calib_mag_start(void)
{
    /* 解锁 -> 进入磁场校准（CALSW=0x07），不保存，等用户旋转 */
    jy901b_write_reg_unlocked(JY901B_REG_CALSW, 0x0007U);
}

void jy901b_calib_mag_end(void)
{
    /* 解锁 -> 退出校准（CALSW=0x00） -> 保存 */
    jy901b_write_reg_raw(JY901B_REG_KEY, JY901B_UNLOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(200U));
    jy901b_write_reg_raw(JY901B_REG_CALSW, 0x0000U);
    vTaskDelay(pdMS_TO_TICKS(200U));
    jy901b_write_reg_raw(JY901B_REG_SAVE, 0x0000U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}
