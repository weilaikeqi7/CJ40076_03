/**
 * @file bsp_power.h
 * @brief 各外设独立供电使能引脚驱动（Ranger/Compass/GNSS/LCD）
 *
 * 职责边界：
 *   - 仅负责外设供电开关引脚的电平使能与断电；
 *   - 不包含上电等待稳定、串口缓存清空等设备级业务时序。
 */
#ifndef BSP_POWER_H
#define BSP_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** 初始化全部外设供电引脚为输出，默认全部断电 */
void bsp_power_init(void);

/* 以下开关须先调用 bsp_power_init；on=true 供电、false 断电，不等待电源稳定。 */
void bsp_pwr_ranger(bool on);  /* 测距机电源开关（PB3，高有效） */
void bsp_pwr_compass(bool on); /* 电子罗盘电源开关（PA8，高有效） */
void bsp_pwr_gnss(bool on);    /* GNSS 电源开关（PB15，高有效） */
void bsp_pwr_lcd(bool on);     /* 显示屏电源开关（PB4，高有效） */

#ifdef __cplusplus
}
#endif

#endif /* BSP_POWER_H */
