/**
 * @file app_display.h
 * @brief OLED 业务渲染（02版本，FO-DM0001显示屏）
 *
 * 显示策略（2秒翻页）：
 *   主页面（2s）：航向 + 距离 + 计数
 *   副页面1（2s）：俯仰 + 高程 + 纬度
 *   副页面2（2s）：经度（坐标分两次显示）
 *
 * 区域划分（数码管编号见 lcd_map.h）：
 *   上排（1-5）：4位整数+1位小数，小数点S8
 *   中排（6-10）：4位整数+1位小数，小数点S25
 *   下排（11-15）：4位整数+1位小数，小数点S30
 *
 * 符号映射：
 *   电池：S4(框) + S1/S2/S3(格)
 *   F/E图标：S5(WiFi)/S7(蓝牙) 借用
 *   定位/靶心：S6(定位)/S16(靶心)
 *   十字准星：S17-S19
 */
#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_coord.h"
#include "app_measure.h"

/** 校准页（渲染用，与 app_calib 状态对应） */
typedef enum
{
    DISP_PAGE_NONE = 0,
    DISP_PAGE_PIT,
    DISP_PAGE_HIT,
    DISP_PAGE_HER,
    DISP_PAGE_FULL_ON, /* JY901B 内部校准中：全显 */
} disp_page_t;

/** 显示页面（2秒翻页） */
typedef enum
{
    DISP_SCREEN_MAIN = 0,  /* 主页面：航向+距离+计数 */
    DISP_SCREEN_SUB1,      /* 副页面1：俯仰+高程+纬度 */
    DISP_SCREEN_SUB2,      /* 副页面2：经度 */
} disp_screen_t;

typedef struct
{
    meas_mode_t mode;       /* 当前模式 */
    bool        measuring;  /* 有轮次进行中（距离区清结果/横杠） */

    const measure_result_t* result; /* 最近一轮结果 */

    bool    att_valid;
    int32_t heading_c01;
    int32_t pitch_c01;

    bool           self_valid;   /* 本机定位有效 */
    app_geo_point_t self;

    bool           target_valid; /* 存在已发布的目标坐标（多功能/测试） */
    app_geo_point_t target_near; /* 首目标坐标 */
    app_geo_point_t target_far;  /* 末目标坐标 */

    uint32_t count;      /* 测量计数 0~9999 */
    uint8_t  batt_level; /* 1~4（4 段） */

    disp_page_t page;          /* 校准页 */
    int16_t     page_value_c01; /* 当前页补偿值（0.01°） */
} disp_state_t;

void display_init(void);

/** 100ms 周期渲染（内部完成 lcd_clear + 各区绘制 + lcd_flush） */
void display_render(const disp_state_t* s);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_H */
