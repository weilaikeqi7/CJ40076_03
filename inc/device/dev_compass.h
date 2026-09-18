/**
 * @file dev_compass.h
 * @brief JY901B 姿态传感器设备接口（USART2，9600 8N1）
 *
 * JY901B 使用 0x55 帧头、TYPE、8 字节数据和 8 位累加和校验。
 * 本接口保留统一设备层文件名，但采用 JY901B 自身的寄存器校准流程。
 */
#ifndef DEV_COMPASS_H
#define DEV_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float    heading;    /* JY901B Yaw，已归一化到 0~360° */
    float    pitch;      /* JY901B Pitch，按本机安装方向换算 */
    float    roll;       /* JY901B Roll */
    uint32_t tick_angle; /* 最近一次收到角度帧的系统 tick */
} jy901b_data_t;

void jy901b_power_ctl(bool on);
bool jy901b_self_check(uint32_t timeout_ms);
void jy901b_init(void);
void jy901b_poll(void);
const jy901b_data_t* jy901b_get_data(void);
bool jy901b_is_alive(uint32_t timeout_ms);

/* JY901B 校准流程：均为寄存器 CALSW/SAVE 操作，不产生采样点或评分。 */
void jy901b_calib_mag_start(void);
void jy901b_calib_mag_end(void);
void jy901b_calib_accel(void);
void jy901b_calib_angle_ref(void);
void jy901b_calib_yaw_zero(void);
void jy901b_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DEV_COMPASS_H */
