/**
 * @file dev_compass.h
 * @brief 三型号通用罗盘设备接口。
 */
#ifndef DEV_COMPASS_H
#define DEV_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "compass_config.h"

typedef struct
{
    float heading;       /* 安装后的航向角，范围为 [0, 360)。 */
    float pitch;         /* 安装后的俯仰角；JY 协议的符号在驱动中转换。 */
    float roll;
    uint32_t tick_angle; /* 最近一次完整有效姿态帧；断电时为 0。 */
} compass_data_t;

typedef struct
{
    uint32_t sample_count;
    float cal_score;
    bool score_valid; /* 已收到有限浮点评分，但不代表评分达到保存阈值。 */
} compass_cal_state_t;

const char* compass_model_name(void);
void compass_power_ctl(bool on);
void compass_init(void);
void compass_poll(void);
bool compass_self_check(uint32_t timeout_ms);
bool compass_is_alive(uint32_t timeout_ms);
/* 上电稳定时间由 compass_step() 推进；本接口绝不阻塞。 */
bool compass_is_ready(void);
const compass_data_t* compass_get_data(void);
void compass_get_data_snapshot(compass_data_t* out);

bool compass_mag_uses_samples(void);
const compass_cal_state_t* compass_get_cal_state(void);
void compass_get_cal_state_snapshot(compass_cal_state_t* out);
void compass_calib_mag_start(void);
void compass_calib_mag_end(void);
void compass_calib_take_sample(void);
bool compass_calib_score_valid(float score);
/* 已接受的命令由按键任务通过 compass_step() 推进，不执行长时间延时。 */
bool compass_factory_reset(void);
bool compass_calib_accel(void);
bool compass_calib_angle_ref(void);
bool compass_is_busy(void);
void compass_step(void);

#ifdef __cplusplus
}
#endif

#endif /* DEV_COMPASS_H */
