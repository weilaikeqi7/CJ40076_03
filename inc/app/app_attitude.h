/**
 * @file app_attitude.h
 * @brief Selected compass angles -> compensated pitch/heading (0.01 degrees).
 *
 * The device layer preserves each model's installed pitch/heading convention.
 * This layer only adds PIt/HIt/HEr host offsets and normalizes the ranges.
 */
#ifndef APP_ATTITUDE_H
#define APP_ATTITUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_store.h"

/** Sensor-task update: poll the selected compass and compensate fresh angles. */
void attitude_update(void);

/** 姿态是否有效（超时内收到角度帧） */
bool attitude_valid(void);

/** 补偿后俯仰角，单位 0.01°（-9000~+9000） */
int32_t attitude_pitch_c01(void);

/** 补偿后航向角，单位 0.01°（0~35999） */
int32_t attitude_heading_c01(void);

/** 补偿值读写（RAM 实时生效；保存到 Flash 走 store_save_offsets） */
void attitude_get_offsets(app_offsets_t* out);
void attitude_set_offsets(const app_offsets_t* offsets);

/** 加载 Flash 中的补偿值（store_init 之后调用） */
void attitude_load_offsets(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ATTITUDE_H */
