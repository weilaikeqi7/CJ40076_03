/**
 * @file jy901b.h
 * @brief JY901B 姿态传感器驱动（USART2 PA2/PA3，9600 8N1）
 *
 * 协议（WitMotion）：
 *   上行数据帧：0x55 + TYPE + 8 字节数据 + SUM（共 11 字节），
 *               SUM = (0x55+TYPE+8字节数据) 低 8 位。
 *   下行写寄存器：0xFF 0xAA ADDR DATAL DATAH；
 *               写操作需先解锁（KEY=0xB588），最后保存（SAVE=0）。
 *
 * 本模块为垂直安装：初始化时自动写 ORIENT=1（垂直安装，Y 轴箭头朝上）。
 *
 * 使用：jy901b_init()（任务上下文，内含延时与上电等待）
 *       主循环周期性调用 jy901b_poll() 喂串口数据，然后用
 *       jy901b_get_data() 取最新姿态。
 */
#ifndef JY901B_H
#define JY901B_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 输出内容 RSW 位定义 */
#define JY901B_RSW_TIME  0x0001U
#define JY901B_RSW_ACC   0x0002U
#define JY901B_RSW_GYRO  0x0004U
#define JY901B_RSW_ANGLE 0x0008U
#define JY901B_RSW_MAG   0x0010U
#define JY901B_RSW_PORT  0x0020U

/** 输出速率 RRATE */
typedef enum
{
    JY901B_RATE_0_2HZ = 0x01,
    JY901B_RATE_0_5HZ = 0x02,
    JY901B_RATE_1HZ   = 0x03,
    JY901B_RATE_2HZ   = 0x04,
    JY901B_RATE_5HZ   = 0x05,
    JY901B_RATE_10HZ  = 0x06,
    JY901B_RATE_20HZ  = 0x07,
    JY901B_RATE_50HZ  = 0x08,
    JY901B_RATE_100HZ = 0x09,
    JY901B_RATE_200HZ = 0x0B,
} jy901b_rate_t;

typedef struct
{
    /* 姿态角，单位 度 */
    float roll;  /* 横滚角 X */
    float pitch; /* 俯仰角 Y */
    float yaw;   /* 航向角 Z */

    /* 加速度，单位 g */
    float acc_x;
    float acc_y;
    float acc_z;

    /* 角速度，单位 度/秒 */
    float gyro_x;
    float gyro_y;
    float gyro_z;

    /* 磁场原始值 */
    int16_t mag_x;
    int16_t mag_y;
    int16_t mag_z;

    float    temp_c;  /* 模块温度，℃（随加速度/磁场帧更新） */
    uint16_t version; /* 固件版本（随角度帧更新） */

    /* 各类数据最近一次更新的系统 tick（0 = 从未收到） */
    uint32_t tick_angle;
    uint32_t tick_acc;
    uint32_t tick_gyro;
    uint32_t tick_mag;
} jy901b_data_t;

/**
 * @brief 上电并初始化 JY901B：开电源(PA8) -> 等待启动 ->
 *        配置垂直安装(ORIENT=1) -> 配置输出内容与速率 -> 保存。
 * @note  必须在 FreeRTOS 任务上下文调用（内部有 vTaskDelay）。
 * @param rate    输出速率（9600 波特率下建议不超过 20Hz）
 * @param content 输出内容掩码（JY901B_RSW_xxx 组合）
 */
void jy901b_init(jy901b_rate_t rate, uint16_t content);

/** 喂串口数据解析数据帧，主循环周期调用 */
void jy901b_poll(void);

/** 取最新数据（只读指针） */
const jy901b_data_t* jy901b_get_data(void);

/** 数据是否在超时内更新过（用于判断模块在线） */
bool jy901b_is_alive(uint32_t timeout_ms);

/* ------------------------- 配置与校准命令 ------------------------- */
/* 均需在任务上下文调用，内部完成 解锁->写入->保存 流程 */

void jy901b_set_orient_vertical(bool vertical); /* true=垂直安装 */
void jy901b_calib_accel(void);                  /* 加速度校准（模块正面朝上静置，约4s） */
void jy901b_set_angle_ref(void);                /* 角度参考（当前位置 XY 归零，约3s） */
void jy901b_yaw_zero(void);                     /* Z 轴航向角置零（仅六轴算法，约3s） */

/**
 * @brief 恢复 JY901B 出厂设置（SAVE=0x0001），随后等待模块重启。
 * @note  恢复后会重新执行当前项目要求的垂直安装、5Hz角度帧配置。
 */
void jy901b_factory_reset(void);

/**
 * @brief 磁场校准开始（球型拟合法，CALSW=0x07）。
 *        调用后需持设备绕三个轴各旋转数圈，完成后调用 jy901b_calib_mag_end()。
 * @note  校准过程不自动退出计时，但建议 5 分钟内完成三轴旋转。
 */
void jy901b_calib_mag_start(void);

/**
 * @brief 磁场校准结束并保存（退出校准 CALSW=0x00 -> 保存）。
 */
void jy901b_calib_mag_end(void);

#ifdef __cplusplus
}
#endif

#endif /* JY901B_H */
