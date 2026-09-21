/**
 * @file app_thermal.c
 * @brief 极低温环境（-40°C）屏幕加热与热管理闭环控制实现
 */
#include "app_thermal.h"

#include "app_config.h"
#include "bsp_adc.h"
#include "dev_heater.h"
#include "debug_log.h"

void app_thermal_init(void)
{
    dev_heater_init();
    DBG_LOGI("[STATE][THERM] heater=off reason=init\r\n");
}

void app_thermal_off(void)
{
    dev_heater_off();
    DBG_LOGI("[STATE][THERM] heater=off reason=power_off\r\n");
}

void app_thermal_step(uint32_t vbat_mv)
{
    int16_t  temp_c10 = bsp_ntc_temperature_c10();
    uint16_t duty     = 0U;
    uint8_t  state    = 0U;
    const char* reason = "off";
#if !ENABLE_DEBUG_LOG
    (void)state;
    (void)reason;
#endif

    /* 温度换算返回无效标记时立即关闭；不使用失效读数继续计算占空比。 */
    if (temp_c10 == BSP_TEMP_INVALID)
    {
        dev_heater_off();
        DBG_LOGW("[FAULT][THERM] temp_invalid vbat_mv=%lu duty_permille=0\r\n",
                 (unsigned long)vbat_mv);
        return;
    }

    /* 2. 电池跌落自保护：带载电压低于安全门限（3.15V）强制断开加热，保住主控供电不复位 */
    if (vbat_mv < APP_HEATER_VBAT_SAFE_MV)
    {
        dev_heater_off();
        DBG_LOGW("[FAULT][THERM] low_battery temp_c10=%d vbat_mv=%lu duty_permille=0\r\n",
                 (int)temp_c10, (unsigned long)vbat_mv);
        return;
    }

    /* 3. 温度分级自适应调节 */
    if (temp_c10 >= APP_HEATER_TEMP_OFF_C10)
    {
        /* 超过 15.0℃：彻底关闭加热 */
        duty = 0U;
        state = 1U;
        reason = "temp_off";
    }
    else if (temp_c10 >= APP_HEATER_TEMP_WARM_C10)
    {
        /* 0.0℃ ~ 15.0℃：维持保温，15% 占空比 */
        duty = APP_HEATER_DUTY_KEEP_WARM;
        state = 2U;
        reason = "keep_warm";
    }
    else if (temp_c10 >= APP_HEATER_TEMP_COLD_C10)
    {
        /* -15.0℃ ~ 0.0℃：快速升温，35% 占空比 */
        duty = APP_HEATER_DUTY_WARM_UP;
        state = 3U;
        reason = "warm_up";
    }
    else
    {
        /* <-15.0℃（极寒至 -40℃）：根据电池充裕程度自适应 */
        if (vbat_mv >= APP_HEATER_VBAT_RICH_MV)
        {
            duty = APP_HEATER_DUTY_COLD_HIGH; /* 电量充足：40% 快速破冰升温 */
            state = 4U;
            reason = "cold_high";
        }
        else
        {
            duty = APP_HEATER_DUTY_COLD_LOW;  /* 电量中等：20% 温和预热唤醒，防止拉垮高内阻电池 */
            state = 5U;
            reason = "cold_low";
        }
    }

    dev_heater_set_power(duty);
    DBG_LOGI("[STATUS][THERM] temp_c10=%d vbat_mv=%lu duty_permille=%u state=%u reason=%s\r\n",
             (int)temp_c10, (unsigned long)vbat_mv, (unsigned int)duty, (unsigned int)state, reason);
}
