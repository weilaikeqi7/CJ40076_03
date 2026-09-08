#include "main.h"

#include "board.h"
#include "board_adc.h"
#include "gnss.h"
#include "jy901b.h"
#include "lcd.h"
#include "lcd_map.h"
#include "ranger.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

#if defined(N32_EXPECT_FPU) && (N32_EXPECT_FPU == 1)
#if (__FPU_USED != 1)
#error "FPU was requested, but CMSIS reports __FPU_USED != 1. Check -mfpu and -mfloat-abi."
#endif
#endif

/**
 * @brief LCD 上电自检：全亮 -> 全灭 -> 棋盘 0xAA -> 棋盘 0x55
 *        （对应原厂 DEMO240.C 的四段测试画面）
 */
static void lcd_self_test(void)
{
    uint8_t* frame = lcd_frame_buffer();
    uint8_t  i;

    lcd_fill();
    lcd_flush();
    vTaskDelay(pdMS_TO_TICKS(1000U));

    lcd_clear();
    lcd_flush();
    vTaskDelay(pdMS_TO_TICKS(500U));

    for (i = 0U; i < LCD_FRAME_BYTES; i++)
    {
        frame[i] = 0xAAU;
    }
    frame[0] = 0x00U;
    lcd_flush();
    vTaskDelay(pdMS_TO_TICKS(500U));

    for (i = 0U; i < LCD_FRAME_BYTES; i++)
    {
        frame[i] = 0x55U;
    }
    frame[0] = 0x00U;
    lcd_flush();
    vTaskDelay(pdMS_TO_TICKS(500U));
}

/**
 * @brief 主应用任务（数码管分组按 P1237 布局）：
 *   - 顶行 1~3：GNSS 解算卫星数；电池框+电量格
 *   - 第二行 4~7：JY901B 俯仰角 x10 的绝对值
 *   - 大字行 8~16：测距结果 x10（米），模式键触发单次测距
 *   - 底行右下 21~24：电池电压（0.01V）
 *   - 电源键：切换加热丝 30% 占空比；十字准星闪烁
 */
static void app_task(void* argument)
{
    (void)argument;

    const gnss_data_t*   gnss;
    const jy901b_data_t* imu;
    ranger_range_t       range      = {0};
    bool                 heater_on  = false;
    bool                 cross_on   = true;
    bool                 mode_prev  = false;
    bool                 power_prev = false;

    board_adc_init();

    gnss_init();   /* BV-220，115200 8N1 */
    jy901b_init(JY901B_RATE_10HZ,
                (uint16_t)(JY901B_RSW_ACC | JY901B_RSW_GYRO | JY901B_RSW_ANGLE)); /* 垂直安装已在内部配置 */
    ranger_init(); /* DYC-15A，115200 8N1，内含 1.6s 上电等待 */

    lcd_init();
    lcd_power_on();
    lcd_self_test();

    while (1)
    {
        gnss_poll();
        jy901b_poll();
        ranger_poll();

        gnss = gnss_get_data();
        imu  = jy901b_get_data();

        /* 模式键：触发单次测距 */
        if (board_key_mode_pressed() && !mode_prev)
        {
            ranger_range_single();
        }
        mode_prev = board_key_mode_pressed();

        /* 电源键：切换加热丝 30% */
        if (board_key_power_pressed() && !power_prev)
        {
            heater_on = !heater_on;
            board_heater_set_duty(heater_on ? 300U : 0U);
        }
        power_prev = board_key_power_pressed();

        /* ------- 刷新 LCD ------- */
        {
            uint32_t batt_mv    = board_battery_mv();
            uint32_t batt_centi = batt_mv / 10U; /* 0.01V 单位，如 745 = 7.45V */

            lcd_clear();

            /* 顶行 1~3：卫星数；电池框 + 按电压粗分三格电量 */
            lcd_print_uint(1, 3, gnss->sats_used, false);
            lcd_symbol(LCD_SYM_BATTERY, true);
            lcd_symbol(LCD_SYM_BAT_BAR1, batt_mv > 3600U);
            lcd_symbol(LCD_SYM_BAT_BAR2, batt_mv > 3800U);
            lcd_symbol(LCD_SYM_BAT_BAR3, batt_mv > 4000U);

            /* 第二行 4~7：俯仰角 x10 绝对值 */
            {
                int32_t pitch_x10 = (int32_t)(imu->pitch * 10.0f);
                if (pitch_x10 < 0)
                {
                    pitch_x10 = -pitch_x10;
                }
                lcd_print_uint(4, 4, (uint32_t)pitch_x10, false);
            }

            /* 大字行 8~16：距离 x10（米） */
            (void)ranger_get_range(&range);
            lcd_print_uint(8, 9, (uint32_t)(range.distance_m * 10.0f), false);

            /* 底行右下 21~24：电池电压 0.01V */
            lcd_print_uint(21, 4, batt_centi, false);

            lcd_symbol(LCD_SYM_CROSSHAIR, cross_on);
            cross_on = !cross_on;

            lcd_flush();
        }

        vTaskDelay(pdMS_TO_TICKS(500U));
    }
}

int main(void)
{
    BaseType_t created;

    /* 最先初始化 GPIO 并保持电源（含电源保持脚置高） */
    board_gpio_init();

    created = xTaskCreate(app_task, "APP", configMINIMAL_STACK_SIZE * 6U, NULL, tskIDLE_PRIORITY + 1U, NULL);
    if (created != pdPASS)
    {
        Error_Handler();
    }

    vTaskStartScheduler();
    Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

void AppAssertFailed(const char* file, int line)
{
    (void)file;
    (void)line;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* task_name)
{
    (void)task;
    (void)task_name;
    Error_Handler();
}

#ifdef USE_FULL_ASSERT
void assert_failed(const uint8_t* expr, const uint8_t* file, uint32_t line)
{
    (void)expr;
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
