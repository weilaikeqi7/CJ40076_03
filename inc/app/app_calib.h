/**
 * @file app_calib.h
 * @brief 硬件校准与通用 PIt/HIt/HEr 补偿状态机。
 *
 * 四击进入 HEr，七击进入 PIt，再长按双键进入 HIt，最后保存退出。
 * 五击进入磁场校准，六击结束并按型号策略保存或放弃。JY901B 连续校准；
 * MCP406/MCG505 使用短按电源键采样，直到设备返回评分。八/九击仅 JY901B 支持。
 * 十击启动异步恢复出厂，完成后恢复主机默认补偿值。
 * 硬件操作期间忽略按键；应用层优先处理长按关机。
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
    CALIB_NONE = 0,
    CALIB_PIT,
    CALIB_HIT,
    CALIB_HER,
    CALIB_MAG,
    CALIB_ACC_BUSY,
    CALIB_ANG_BUSY,
    CALIB_FACTORY_BUSY,
} calib_state_t;

calib_state_t calib_get_state(void);
int16_t calib_page_value_c01(void);
/** 消费校准按键并排队命令，不直接切换设备电源。 */
bool calib_handle_key(const app_key_event_t* evt);
/** T_KEY 每轮停止测距并应用供电策略后调用。
 *  推进待发命令和异步操作，执行后重新应用供电策略。
 */
void calib_step(void);
bool calib_mag_in_progress(void);
bool calib_page_active(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIB_H */
