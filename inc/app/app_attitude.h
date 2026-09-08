/**
 * @file app_attitude.h
 * @brief 姿态解算：JY901B 原始角度 -> 补偿后俯仰/航向（0.01 度整数运算）
 *
 * 换算规则（测试手册）：
 *   俯仰 = -原始俯仰 + PIt      （激光朝上为正、朝下为负）
 *   航向 = -原始航向 + HIt + HEr（归一化到 0~359.99°）
 * JY901B 垂直安装（ORIENT=1），5Hz 仅角度帧。
 */
#ifndef APP_ATTITUDE_H
#define APP_ATTITUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_store.h"

/** 主循环周期调用：取 JY901B 最新角度帧并换算 */
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
