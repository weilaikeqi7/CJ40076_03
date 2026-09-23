/**
 * @file app_thermal.c
 * @brief 极低温环境（-40°C）屏幕加热与热管理闭环控制实现
 *
 * 针对 -40°C + ICR18650 的特性：
 *   1. 电池内阻剧增，加热瞬间电压跌落 -> 电压滞环（3150 断 / 3300 恢复）防止 1Hz 打嗝；
 *   2. 冷启动电池未自热 -> 先 10% 自热 N 秒，电压回升后再升档；
 *   3. NTC 在 -40℃ 分辨率低 -> 温度滞环 ±0.5℃ 防档位抖动；
 *   4. 屏幕工作下限 -10℃ -> 快速升温区上移到 -5℃，留 5℃ 余量。
 */
#include "app_thermal.h"

#include "app_config.h"
#include "app_measure.h"
#include "bsp_adc.h"
#include "dev_heater.h"

/** 加热当前是否被电压保护切断（带滞环） */
static bool     s_volt_cut;
/** 冷启动自热剩余秒数 */
static uint8_t  s_prewarm_left;
/** 上一次的档位（用于温度滞环） */
static uint16_t s_last_duty;
/** 当前实际下发的占空比（千分比），供采样暂停/恢复使用 */
static uint16_t s_cur_duty;
/** 是否完成初始化（防止上电顺序中先于 TIM4 初始化被调用） */
static bool     s_inited;

void app_thermal_init(void)
{
    dev_heater_init();
    s_volt_cut     = false;
    s_prewarm_left = 0U;
    s_last_duty    = 0U;
    s_cur_duty     = 0U;
    s_inited       = true;
}

void app_thermal_off(void)
{
    dev_heater_off();
    s_volt_cut     = false;
    s_prewarm_left = 0U;
    s_last_duty    = 0U;
    s_cur_duty     = 0U;
}

void app_thermal_pause(void)
{
    if (!s_inited || s_cur_duty == 0U)
    {
        return;
    }
    dev_heater_set_power(0U);
}

void app_thermal_resume(void)
{
    if (!s_inited || s_cur_duty == 0U)
    {
        return;
    }
    dev_heater_set_power(s_cur_duty);
}

void app_thermal_step(uint32_t vbat_mv)
{
    int16_t  temp_c10 = bsp_ntc_temperature_c10();
    uint16_t duty     = 0U;

    /* 温度换算返回无效标记时立即关闭；不使用失效读数继续计算占空比。 */
    if (temp_c10 == BSP_TEMP_INVALID)
    {
        dev_heater_off();
        s_volt_cut     = false;
        s_prewarm_left = 0U;
        s_last_duty    = 0U;
        s_cur_duty     = 0U;
        return;
    }

    /* 电池电压滞环：3150mV 切断，3300mV 才允许恢复，防止 -40℃ 下 1Hz 打嗝 */
    if (vbat_mv < APP_HEATER_VBAT_SAFE_MV)
    {
        s_volt_cut = true;
    }
    else if (vbat_mv >= APP_HEATER_VBAT_RESUME_MV)
    {
        s_volt_cut = false;
    }

    if (s_volt_cut)
    {
        s_cur_duty = 0U;
        dev_heater_off();
        return;
    }

    /* 测距轮次进行中：激光脉冲电流优先级最高，加热强制降到最低档，
       防止 3.5Ω 加热丝与激光同时拉载导致 -40℃ 电池电压崩塌、整机欠压关机。 */
    if (measure_round_active())
    {
        duty   = APP_HEATER_DUTY_PREWARM;
        s_cur_duty = duty;
        dev_heater_set_power(duty);
        return;
    }

    /* 冷启动自热：-15℃ 以下且电压偏低时，先给电池 10% 小电流自热，再升档 */
    if (temp_c10 < APP_HEATER_TEMP_COLD_C10 && vbat_mv < APP_HEATER_VBAT_RICH_MV &&
        s_prewarm_left > 0U)
    {
        duty = APP_HEATER_DUTY_PREWARM;
        s_prewarm_left--;
        s_cur_duty = duty;
        dev_heater_set_power(duty);
        return;
    }

    /* 温度分级自适应调节，带 ±0.5℃ 滞环：NTC 在 -40℃ 附近每 1℃ 只有 3~4 个
     * ADC 码，分界线附近读数抖动会导致档位来回切、加热忽大忽小。规则：
     * 升档要求温度高出分界线 0.5℃，降档要求低出 0.5℃，带内维持上一档。 */
    if (temp_c10 >= APP_HEATER_TEMP_OFF_C10 + APP_HEATER_TEMP_HYST_C10)
    {
        duty = 0U;                                   /* > 15.5℃：彻底关闭 */
    }
    else if (temp_c10 >= APP_HEATER_TEMP_OFF_C10 - APP_HEATER_TEMP_HYST_C10)
    {
        duty = (s_last_duty == APP_HEATER_DUTY_KEEP_WARM ||
                s_last_duty == APP_HEATER_DUTY_WARM_UP ||
                s_last_duty == APP_HEATER_DUTY_COLD_HIGH ||
                s_last_duty == APP_HEATER_DUTY_COLD_LOW ||
                s_last_duty == APP_HEATER_DUTY_PREWARM)
                   ? s_last_duty                     /* 14.5~15.5℃ 带内：维持 */
                   : APP_HEATER_DUTY_KEEP_WARM;      /* 冷启动在此带：保温 */
    }
    else if (temp_c10 >= APP_HEATER_TEMP_WARM_C10 + APP_HEATER_TEMP_HYST_C10)
    {
        duty = APP_HEATER_DUTY_KEEP_WARM;            /* 0.5~14.5℃：保温 */
    }
    else if (temp_c10 >= APP_HEATER_TEMP_WARM_C10 - APP_HEATER_TEMP_HYST_C10)
    {
        duty = (s_last_duty == APP_HEATER_DUTY_WARM_UP ||
                s_last_duty == APP_HEATER_DUTY_COLD_HIGH ||
                s_last_duty == APP_HEATER_DUTY_COLD_LOW ||
                s_last_duty == APP_HEATER_DUTY_PREWARM)
                   ? s_last_duty                     /* -0.5~0.5℃ 带内：维持 */
                   : APP_HEATER_DUTY_WARM_UP;        /* 从高温降到此带：升温 */
    }
    else if (temp_c10 >= APP_HEATER_TEMP_COLD_C10 + APP_HEATER_TEMP_HYST_C10)
    {
        duty = APP_HEATER_DUTY_WARM_UP;              /* -4.5~-0.5℃：快速升温 */
    }
    else if (temp_c10 >= APP_HEATER_TEMP_COLD_C10 - APP_HEATER_TEMP_HYST_C10)
    {
        duty = (s_last_duty == APP_HEATER_DUTY_COLD_HIGH ||
                s_last_duty == APP_HEATER_DUTY_COLD_LOW ||
                s_last_duty == APP_HEATER_DUTY_PREWARM)
                   ? s_last_duty                     /* -5.5~-4.5℃ 带内：维持 */
                   : APP_HEATER_DUTY_COLD_LOW;       /* 从保温降到此带：极寒低档 */
    }
    else
    {
        /* < -5.5℃（极寒至 -40℃）：根据电池充裕程度自适应 */
        if (vbat_mv >= APP_HEATER_VBAT_RICH_MV)
        {
            duty = APP_HEATER_DUTY_COLD_HIGH;
        }
        else
        {
            duty = APP_HEATER_DUTY_COLD_LOW;
        }
    }

    /* 每次进入极寒区且电压偏低，重置自热计时（下次掉压时可用） */
    if (temp_c10 < APP_HEATER_TEMP_COLD_C10 && vbat_mv < APP_HEATER_VBAT_RICH_MV &&
        s_prewarm_left == 0U && s_last_duty == 0U)
    {
        s_prewarm_left = APP_HEATER_PREWARM_SEC;
    }

    s_last_duty = duty;
    s_cur_duty  = duty;
    dev_heater_set_power(duty);
}
