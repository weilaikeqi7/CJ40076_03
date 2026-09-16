/**
 * @file app_thermal.c
 * @brief 极低温环境（-40°C）屏幕加热与热管理闭环控制实现
 */
#include "app_thermal.h"

#include "app_config.h"
#include "board.h"
#include "board_adc.h"
#include "rtt_log.h"

void app_thermal_init(void)
{
    board_heater_set_duty(0U);
}

void app_thermal_off(void)
{
    board_heater_set_duty(0U);
}

void app_thermal_step(uint32_t vbat_mv)
{
    int16_t  temp_c10 = board_ntc_temperature_c10();
    uint16_t duty     = 0U;

    /* 1. 安全保护：NTC 传感器开路、脱落或短路，彻底关闭加热防止干烧 */
    if (temp_c10 == BOARD_TEMP_INVALID)
    {
        board_heater_set_duty(0U);
        return;
    }

    /* 2. 电池跌落自保护：带载电压低于安全门限（3.15V）强制断开加热，保住主控供电不复位 */
    if (vbat_mv < APP_HEATER_VBAT_SAFE_MV)
    {
        board_heater_set_duty(0U);
        return;
    }

    /* 3. 温度分级自适应调节 */
    if (temp_c10 >= APP_HEATER_TEMP_OFF_C10)
    {
        /* 超过 15.0℃：彻底关闭加热 */
        duty = 0U;
    }
    else if (temp_c10 >= APP_HEATER_TEMP_WARM_C10)
    {
        /* 0.0℃ ~ 15.0℃：维持保温，15% 占空比 */
        duty = APP_HEATER_DUTY_KEEP_WARM;
    }
    else if (temp_c10 >= APP_HEATER_TEMP_COLD_C10)
    {
        /* -15.0℃ ~ 0.0℃：快速升温，35% 占空比 */
        duty = APP_HEATER_DUTY_WARM_UP;
    }
    else
    {
        /* <-15.0℃（极寒至 -40℃）：根据电池充裕程度自适应 */
        if (vbat_mv >= APP_HEATER_VBAT_RICH_MV)
        {
            duty = APP_HEATER_DUTY_COLD_HIGH; /* 电量充足：40% 快速破冰升温 */
        }
        else
        {
            duty = APP_HEATER_DUTY_COLD_LOW;  /* 电量中等：20% 温和预热唤醒，防止拉垮高内阻电池 */
        }
    }

    board_heater_set_duty(duty);
}
