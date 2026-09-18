/**
 * @file app_power.c
 * @brief 系统电源管理、ICR18650 电池状态监测与安全软关机实现
 */
#include "app_power.h"

#include "app_calib.h"
#include "app_config.h"
#include "app_store.h"
#include "app_thermal.h"
#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp_iwdg.h"
#include "dev_compass.h"
#include "dev_display.h"
#include "dev_gnss.h"
#include "dev_ranger.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_batt_mv  = 4200U;
static uint8_t  s_batt_lvl = 4U;

void app_power_init(void)
{
    s_batt_mv  = bsp_battery_mv();
    s_batt_lvl = 0U; /* 置 0 使首次 check 时执行无滞环快速初始化 */
    app_power_check();
}

void app_power_check(void)
{
    /* 滞环回差（mV）：防止电池开路电压在分档临界点抖动跳格 */
    const uint32_t HYST_MV = 30U;
    uint32_t mv = bsp_battery_mv();
    uint8_t  lvl;

    taskENTER_CRITICAL();
    s_batt_mv = mv;
    lvl       = s_batt_lvl;

    if (lvl == 0U)
    {
        /* 初次采样无滞环初始化 */
        if (mv >= APP_BATT_LVL4_MV)
        {
            s_batt_lvl = 4U;
        }
        else if (mv >= APP_BATT_LVL3_MV)
        {
            s_batt_lvl = 3U;
        }
        else if (mv >= APP_BATT_LVL2_MV)
        {
            s_batt_lvl = 2U;
        }
        else
        {
            s_batt_lvl = 1U;
        }
    }
    else
    {
        /* 带回差的分档判定：升级需额外超出回差门限，降级保持原门限 */
        if (mv >= (APP_BATT_LVL4_MV + (lvl < 4U ? HYST_MV : 0U)))
        {
            s_batt_lvl = 4U;
        }
        else if (mv >= (APP_BATT_LVL3_MV + (lvl < 3U ? HYST_MV : 0U)))
        {
            s_batt_lvl = 3U;
        }
        else if (mv >= (APP_BATT_LVL2_MV + (lvl < 2U ? HYST_MV : 0U)))
        {
            s_batt_lvl = 2U;
        }
        else
        {
            s_batt_lvl = 1U;
        }
    }
    taskEXIT_CRITICAL();

    /* 欠压保护：低于 3000mV 自动关机防过放 */
    if (mv < APP_BATT_LOW_OFF_MV)
    {
        LOGI("sys: low battery %lumV, power off\r\n", (unsigned long)mv);
        app_power_shutdown();
    }
}

uint32_t app_power_get_batt_mv(void)
{
    return s_batt_mv;
}

uint8_t app_power_get_batt_lvl(void)
{
    return s_batt_lvl;
}

void app_power_shutdown(void)
{
    /* 磁场校准中长按关机 = 放弃本轮（不发送结束命令） */
    if (calib_mag_in_progress())
    {
        LOGI("calib: mag calibration aborted by power off\r\n");
    }

    LOGI("sys: power off, save count=%lu\r\n", (unsigned long)store_get_count());
    (void)store_save_count(); /* 正常关机与欠压关机均保存计数 */

    lcd_power_off();            /* 先 DISP=0 再断屏电 */
    ranger_power_ctl(false);    /* 测距机下电 */
    mcg505_power_ctl(false);    /* 电子罗盘下电 */
    gnss_power_ctl(false);      /* 卫星定位下电 */
    app_thermal_off();          /* 加热丝强制切断 */

    bsp_power_hold_ctrl(false); /* 释放 PB12，切断整机总电源 */
    while (1)
    {
        /* 关机等待掉电死循环中持续喂狗，防止板载电容残余电压放电期间看门狗超时误触发复位重启 */
        bsp_iwdg_feed();
        for (volatile uint32_t i = 0U; i < 50000U; i++)
        {
            __NOP();
        }
    }
}
