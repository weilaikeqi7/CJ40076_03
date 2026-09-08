/**
 * @file app_display.h
 * @brief LCD 业务渲染（V3 界面，全部字段 1 位小数）
 *
 * 区域划分（数码管编号见 lcd_map.h）：
 *   顶行：  [最高位特殊段] 1 2 . 3  -> 航向 XXX.X°（多功能/测试显示）+ 8 方向罗盘字母
 *           电池框 S15 + 3 格条 S17/S18/S16
 *   第二行：[最高位特殊段] 4 5 6 . 7 -> 距离 XXXX.X M；首末目标图标 F(S29)/E(S30)
 *           模式图标（单次/连续，段号待定见 LCD_ICON_*）
 *   中行：  S37 负号 + 25 26 . 27 -> 俯仰 -XX.X°（P=S36）
 *   大字行：8 9 10 °(S34) 11 12 ′(S33) 13 14 .(S1) 15 16 ″(S2)
 *           -> 坐标 DDD° MM′ SS.ss″，经纬度 1s 交替，半球用 WSEN 字母指示
 *           本机/目标指示：定位图标 S6=LOCAL，靶心图标 S5=TARGET
 *   底行：  H(S45) [最高位特殊段] 17 18 19 . 20 -> 高程 XXXX.X M(S49)（负值取绝对值）
 *           21 22 23 24 -> 计数 0~9999
 *
 * F/E 交替：单次/连续 1s；多功能/测试 2s（坐标/高程跟随 F/E 同步交替，
 *           坐标行内部经纬度 1s 交替，形成 2s 嵌套 1s 的 4s 循环）。
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
