/**
 * @file app_calib.h
 * @brief 校准与补偿设置状态机（适配 MCP-406-TTL 电子罗盘）
 *
 * 角度补偿设置（主控 Flash）：
 *   七击 -> PIt 页；四击 -> HEr 页
 *   页内：电源键 +0.1°，模式键 -0.1°（按住 800ms 后每 100ms 连调）
 *   双键同按 1s：PIt 页 -> 切 HIt 页；HIt/HEr 页 -> 保存 Flash 并退出
 *   补偿值显示在高程区（绝对值，0.1°），实时生效值显示在各自区域
 *
 * MCP-406 磁场空间手动校准：
 *   五击 -> 启动磁场空间手动校准（进入校准页，非全显；已采点数=1，总点数=12）
 *   短按电源键 -> 发送单次采样指令（采样点累加；采样完成罗盘返回得分）
 *   六击 -> 退出校准页面：
 *          - 未采样完：发送校准停止指令，不发送保存指令
 *          - 采样完但得分异常：不发送保存指令
 *          - 采样完且得分正常：发送保存指令至罗盘 EEPROM
 *   十击 -> 罗盘恢复出厂设置并重新写入当前项目配置
 *   （八击加速度校准和九击角度校准已删除）
 */
#ifndef APP_CALIB_H
#define APP_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_key.h"
#include "app_store.h"

typedef enum
{
    CALIB_NONE = 0, /* 正常运行 */
    CALIB_PIT,      /* PIt 补偿页 */
    CALIB_HIT,      /* HIt 补偿页 */
    CALIB_HER,      /* HEr 补偿页 */
    CALIB_MAG,      /* 磁场空间手动校准进行中 */
} calib_state_t;

/** 当前校准状态 */
calib_state_t calib_get_state(void);

/** 当前页补偿值（0.01°），非设置页返回 0 */
int16_t calib_page_value_c01(void);

/**
 * @brief 处理按键事件（模式键多击/校准页单击连发/双键长按/电源键采样）。
 * @return true = 事件被校准模块消费（app 不再按正常业务处理）
 */
bool calib_handle_key(const app_key_event_t* evt);

/** 磁场校准中关机：放弃本轮（返回 true 表示正处于磁场校准） */
bool calib_mag_in_progress(void);

/** 设置页是否激活（用于供电调度：GNSS 关、罗盘保） */
bool calib_page_active(void);

/* --------------------- 磁场校准显示数据查询接口 --------------------- */

/** 获取当前已采样点数（从 1 开始累加） */
uint16_t calib_mag_cur_samples(void);

/** 获取总采样点数（默认 12） */
uint16_t calib_mag_total_samples(void);

/**
 * @brief 获取磁场校准得分
 * @param out_score 输出得分值（Float32）
 * @return true = 采样完成且已获得有效得分，false = 尚未完成
 */
bool calib_mag_get_score(float* out_score);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIB_H */
