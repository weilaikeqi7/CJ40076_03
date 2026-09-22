/**
 * @file app_power.c
 * @brief 系统电源管理、ICR18650 电池状态监测与安全软关机实现
 */
#include "app_power.h"

#include "app.h"
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
#include "debug_log.h"

#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_batt_mv  = 4200U;
static uint8_t  s_batt_lvl = 4U;
static volatile bool s_shutting_down;
#if ENABLE_DEBUG_LOG
static uint8_t s_last_logged_lvl;
static uint32_t s_last_logged_mv;
#endif

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
    uint32_t mv;
    uint8_t  lvl;

    /* 采样前暂停加热 PWM：3.5Ω 加热丝导通电流会显著拉低电池电压，
       暂停约 6 点滤波采样的几毫秒对屏幕温度无影响（热惯性以分钟计）。
       注意：测距激光脉冲期间不做避让，带载读数本就是欠压判据所需的应力值。 */
    app_thermal_pause();
    mv = bsp_battery_mv();
    app_thermal_resume();

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

#if ENABLE_DEBUG_LOG
    if (s_batt_lvl != s_last_logged_lvl ||
        (mv > s_last_logged_mv ? mv - s_last_logged_mv : s_last_logged_mv - mv) >= 20U)
    {
        LOG_POWER("[STATUS][POWER] batt_mv=%lumV batt_lvl=%u\r\n", (unsigned long)mv,
                 (unsigned int)s_batt_lvl);
        s_last_logged_lvl = s_batt_lvl;
        s_last_logged_mv = mv;
    }
#endif
    if (mv == 0U)
    {
        LOG_POWER("[FAULT][POWER] battery_adc_invalid\r\n");
    }

    /* 欠压保护：低于 3000mV 自动关机防过放 */
    if (mv < APP_BATT_LOW_OFF_MV)
    {
        LOG_POWER("sys: low battery %lumV, power off\r\n", (unsigned long)mv);
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

bool app_power_is_shutting_down(void)
{
    return s_shutting_down;
}

void app_power_shutdown(void)
{
    /* 先锁存关机并停止其他任务，阻止旧的上电或校准流程恢复执行。 */
    taskENTER_CRITICAL();
    s_shutting_down = true;
    app_stop_tasks();
    taskEXIT_CRITICAL();

    /* 此后仅当前任务执行收尾，串口、屏幕与外设供电不会被其他任务重启。 */
    /* 磁场校准中关机视为放弃本轮，不发送结束或保存命令。 */
    if (calib_mag_in_progress())
    {
        LOG_CALIB("calib: mag calibration aborted by power off\r\n");
    }

    LOG_POWER("sys: power off, save count=%lu\r\n", (unsigned long)store_get_count());
    (void)store_save_count(); /* 正常关机与欠压关机均保存计数 */

    lcd_power_off();            /* 先 DISP=0 再断屏电 */
    ranger_power_ctl(false);    /* 测距机下电 */
    compass_power_ctl(false);   /* 电子罗盘下电 */
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
