/**
 * @file app_calib.h
 * @brief JY901B 校准与主控姿态补偿设置状态机
 *
 * 四击进入 HEr，七击进入 PIt；五击开始 JY901B 磁场校准，六击结束并保存。
 * 八击执行加速度校准，九击执行角度参考，十击恢复 JY901B 出厂设置。
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
bool calib_handle_key(const app_key_event_t* evt);
bool calib_mag_in_progress(void);
bool calib_page_active(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIB_H */
