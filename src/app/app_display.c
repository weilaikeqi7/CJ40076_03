/**
 * @file app_display.c
 * @brief OLED 业务渲染实现（02版本，FO-DM0001显示屏）
 *
 * 显示策略：
 *   - 2秒翻页：主页面 -> 副页面1 -> 副页面2 -> 主页面
 *   - 主页面：航向(上排) + 距离(中排) + 计数(下排)
 *   - 副页面1：俯仰(上排) + 高程(中排) + 纬度(下排)
 *   - 副页面2：经度(上排+中排)
 *   - F/E交替：单次/连续1s；多功能/测试2s
 */
#include "app_display.h"

#include "app_config.h"
#include "lcd.h"
#include "lcd_map.h"

#include <string.h>

/** 翻页周期（ms） */
#define DISP_SCREEN_PERIOD_MS 2000U

/** F/E交替周期 */
#define DISP_FE_TOGGLE_FAST_MS 1000U
#define DISP_FE_TOGGLE_SLOW_MS 2000U

/* ------------------------------ 数字绘制辅助 ------------------------------ */

/**
 * @brief 绘制航向角（XXX.X°，3位整数+1位小数）
 */
static void draw_heading(uint8_t first_digit, int32_t heading_c01, bool valid)
{
    uint32_t h_x1;

    if (!valid)
    {
        lcd_print_x3_1(first_digit, 0, LCD_SYM_DOT_TOP, false);
        lcd_symbol(LCD_SYM_ANGLE, false);
        return;
    }

    /* 归一化到0-359.99° */
    heading_c01 %= APP_HEADING_PERIOD_C01;
    if (heading_c01 < 0)
    {
        heading_c01 += APP_HEADING_PERIOD_C01;
    }

    h_x1 = (uint32_t)heading_c01 / 10U; /* 转为0.1°单位 */
    lcd_print_x3_1(first_digit, h_x1, LCD_SYM_DOT_TOP, true);
    lcd_symbol(LCD_SYM_ANGLE, true); /* 角度符号 */
}

/**
 * @brief 绘制距离（XXXX.XM，4位整数+1位小数）
 */
static void draw_distance(uint8_t first_digit, uint32_t dist_mm, bool valid)
{
    uint32_t dist_x1m;

    if (!valid)
    {
        lcd_print_x4_1(first_digit, 0, LCD_SYM_DOT_MID, false);
        lcd_symbol(LCD_SYM_UNIT_M1, false);
        return;
    }

    dist_x1m = dist_mm / 100U; /* 转为0.1m单位 */
    lcd_print_x4_1(first_digit, dist_x1m, LCD_SYM_DOT_MID, true);
    lcd_symbol(LCD_SYM_UNIT_M1, true); /* m单位 */
}

/**
 * @brief 绘制俯仰角（-XX.X°，2位整数+1位小数+负号）
 */
static void draw_pitch(uint8_t first_digit, int32_t pitch_c01, bool valid)
{
    uint32_t abs_x1;
    uint32_t int_part;
    uint8_t  frac;

    if (!valid)
    {
        lcd_print_digit(first_digit, -1);
        lcd_print_digit((uint8_t)(first_digit + 1U), -1);
        lcd_print_digit((uint8_t)(first_digit + 2U), -1);
        lcd_symbol(LCD_SYM_DOT_TOP, false);
        lcd_symbol(LCD_SYM_ANGLE, false);
        return;
    }

    abs_x1   = (uint32_t)((pitch_c01 < 0) ? -pitch_c01 : pitch_c01) / 10U; /* 0.1° */
    int_part = abs_x1 / 10U;
    frac     = (uint8_t)(abs_x1 % 10U);

    /* 负号用数码管显示"-" */
    if (pitch_c01 < 0)
    {
        lcd_print_digit(first_digit, -2); /* 显示"-" */
    }
    else
    {
        lcd_print_digit(first_digit, -1); /* 空白 */
    }

    /* 2位整数 */
    lcd_print_digit((uint8_t)(first_digit + 1U), (int8_t)(int_part / 10U));
    lcd_print_digit((uint8_t)(first_digit + 2U), (int8_t)(int_part % 10U));

    /* 小数点 */
    lcd_symbol(LCD_SYM_DOT_TOP, true);

    /* 1位小数 */
    lcd_print_digit((uint8_t)(first_digit + 3U), (int8_t)frac);

    /* 角度符号 */
    lcd_symbol(LCD_SYM_ANGLE, true);
}

/**
 * @brief 绘制高程（XXXX.XM，4位整数+1位小数）
 */
static void draw_altitude(uint8_t first_digit, float alt_m, bool valid)
{
    uint32_t alt_x1m;

    if (!valid)
    {
        lcd_print_x4_1(first_digit, 0, LCD_SYM_DOT_MID, false);
        lcd_symbol(LCD_SYM_UNIT_M1, false);
        return;
    }

    /* 取绝对值并转为0.1m单位 */
    alt_x1m = (uint32_t)((alt_m < 0.0f) ? -alt_m : alt_m) * 10.0f;
    lcd_print_x4_1(first_digit, alt_x1m, LCD_SYM_DOT_MID, true);
    lcd_symbol(LCD_SYM_UNIT_M1, true);
}

/**
 * @brief 绘制坐标分量（DD°MM′或DDD°MM′，压缩到5位显示）
 */
static void draw_coord_part(uint8_t first_digit, double deg, bool is_lon, bool valid)
{
    uint32_t deg_i, min_i;
    double   abs_val;

    if (!valid)
    {
        uint8_t i;
        for (i = 0U; i < 5U; i++)
        {
            lcd_print_digit((uint8_t)(first_digit + i), -2);
        }
        return;
    }

    abs_val = (deg < 0.0) ? -deg : deg;
    deg_i   = (uint32_t)abs_val;
    min_i   = (uint32_t)((abs_val - (double)deg_i) * 60.0);

    if (is_lon)
    {
        /* 经度：DDD°MM′ -> 显示 DDDMM */
        lcd_print_digit(first_digit, (int8_t)((deg_i / 100U) % 10U));
        lcd_print_digit((uint8_t)(first_digit + 1U), (int8_t)((deg_i / 10U) % 10U));
        lcd_print_digit((uint8_t)(first_digit + 2U), (int8_t)(deg_i % 10U));
        lcd_print_digit((uint8_t)(first_digit + 3U), (int8_t)(min_i / 10U));
        lcd_print_digit((uint8_t)(first_digit + 4U), (int8_t)(min_i % 10U));
    }
    else
    {
        /* 纬度：DD°MM′ -> 显示 DDMM */
        lcd_print_digit(first_digit, -1); /* 空白 */
        lcd_print_digit((uint8_t)(first_digit + 1U), (int8_t)((deg_i / 10U) % 10U));
        lcd_print_digit((uint8_t)(first_digit + 2U), (int8_t)(deg_i % 10U));
        lcd_print_digit((uint8_t)(first_digit + 3U), (int8_t)(min_i / 10U));
        lcd_print_digit((uint8_t)(first_digit + 4U), (int8_t)(min_i % 10U));
    }
}

/**
 * @brief 绘制计数（XXXX，4位整数）
 */
static void draw_count(uint8_t first_digit, uint32_t count)
{
    if (count > APP_COUNT_MAX)
    {
        count = APP_COUNT_MAX;
    }
    lcd_print_uint(first_digit, 4, count, false);
}

/**
 * @brief 绘制电池电量（4段：框+3格），level=1（低电空框）时以 1Hz 闪烁报警
 */
static void draw_battery(uint8_t level)
{
    static uint32_t flash_timer = 0U;
    static bool     flash_state = true;

    flash_timer += APP_DISP_RENDER_MS;
    if (flash_timer >= 500U)
    {
        flash_timer = 0U;
        flash_state = !flash_state;
    }

    /* level=1 为低电濒危状态，外框以 1Hz 闪烁（500ms亮/500ms灭） */
    if (level <= 1U)
    {
        lcd_symbol(LCD_SYM_BATTERY, flash_state);
        lcd_symbol(LCD_SYM_BAT_BAR1, false);
        lcd_symbol(LCD_SYM_BAT_BAR2, false);
        lcd_symbol(LCD_SYM_BAT_BAR3, false);
    }
    else
    {
        lcd_symbol(LCD_SYM_BATTERY, true);
        lcd_symbol(LCD_SYM_BAT_BAR1, level >= 2U);
        lcd_symbol(LCD_SYM_BAT_BAR2, level >= 3U);
        lcd_symbol(LCD_SYM_BAT_BAR3, level >= 4U);
    }
}

/* ------------------------------ 主渲染 ------------------------------ */

void display_init(void)
{
    lcd_init();
    lcd_power_on();
}

void display_render(const disp_state_t* s)
{
    static uint32_t screen_timer = 0U;
    static uint32_t fe_timer     = 0U;
    static disp_screen_t cur_screen = DISP_SCREEN_MAIN;
    static bool fe_show_far = false;

    bool multi_mode = (s->mode == MEAS_MODE_MULTI || s->mode == MEAS_MODE_TEST);
    uint32_t fe_period = multi_mode ? DISP_FE_TOGGLE_SLOW_MS : DISP_FE_TOGGLE_FAST_MS;

    /* 校准页全显 */
    if (s->page == DISP_PAGE_FULL_ON)
    {
        lcd_fill();
        lcd_flush();
        return;
    }

    /* 2秒翻页计时 */
    screen_timer += APP_DISP_RENDER_MS;
    if (screen_timer >= DISP_SCREEN_PERIOD_MS)
    {
        screen_timer = 0U;
        cur_screen = (disp_screen_t)((cur_screen + 1U) % 3U);
    }

    /* F/E交替计时 */
    fe_timer += APP_DISP_RENDER_MS;
    if (fe_timer >= fe_period)
    {
        fe_timer = 0U;
        fe_show_far = !fe_show_far;
    }

    lcd_clear();

    /* ---------------- 校准页（PIt/HIt/HEr） ---------------- */
    if (s->page != DISP_PAGE_NONE)
    {
        /* 补偿值显示在中排（绝对值，0.1°） */
        uint32_t abs_x1 = (uint32_t)(s->page_value_c01 < 0 ? -(int32_t)s->page_value_c01
                                                           : (int32_t)s->page_value_c01) / 10U;

        lcd_print_x4_1(6, abs_x1, LCD_SYM_DOT_MID, true);
        lcd_symbol(LCD_SYM_UNIT_M1, true);

        /* 实时生效值回到各自区域 */
        if (s->page == DISP_PAGE_PIT)
        {
            draw_pitch(1, s->pitch_c01, s->att_valid);
        }
        else
        {
            /* HIt/HEr 页：上排实时航向 */
            draw_heading(1, s->heading_c01, s->att_valid);
        }

        draw_battery(s->batt_level);
        lcd_flush();
        return;
    }

    /* ---------------- 正常模式：翻页显示 ---------------- */
    switch (cur_screen)
    {
    case DISP_SCREEN_MAIN:
        /* 主页面：航向 + 距离 + 计数 */
        if (multi_mode)
        {
            draw_heading(1, s->heading_c01, s->att_valid);
        }

        /* 距离：F/E交替 */
        {
            bool     show_far = fe_show_far && s->result->far_valid;
            bool     dist_ok  = !s->measuring && (show_far ? s->result->far_valid : s->result->near_valid);
            uint32_t dist_mm  = show_far ? s->result->far_mm : s->result->near_mm;

            draw_distance(6, dist_mm, dist_ok);
            lcd_symbol(LCD_SYM_WIFI, dist_ok && !show_far); /* F图标 */
            lcd_symbol(LCD_SYM_BT, dist_ok && show_far);    /* E图标 */
        }

        /* 计数 */
        draw_count(12, s->count);
        break;

    case DISP_SCREEN_SUB1:
        /* 副页面1：俯仰 + 高程 + 纬度 */
        if (multi_mode)
        {
            draw_pitch(1, s->pitch_c01, s->att_valid);

            /* 高程和纬度 */
            {
                bool show_far = fe_show_far && s->result->far_valid && s->target_valid;
                const app_geo_point_t* pos;
                bool pos_valid;

                if (s->target_valid)
                {
                    pos = show_far ? &s->target_far : &s->target_near;
                    pos_valid = pos->valid;
                    lcd_symbol(LCD_SYM_LOCATION, false);
                    lcd_symbol(LCD_SYM_TARGET, true);
                }
                else
                {
                    pos = &s->self;
                    pos_valid = s->self_valid;
                    lcd_symbol(LCD_SYM_LOCATION, true);
                    lcd_symbol(LCD_SYM_TARGET, false);
                }

                draw_altitude(6, pos->altitude_m, pos_valid);
                draw_coord_part(11, pos->latitude, false, pos_valid);
            }
        }
        break;

    case DISP_SCREEN_SUB2:
        /* 副页面2：经度 */
        if (multi_mode)
        {
            bool show_far = fe_show_far && s->result->far_valid && s->target_valid;
            const app_geo_point_t* pos;
            bool pos_valid;

            if (s->target_valid)
            {
                pos = show_far ? &s->target_far : &s->target_near;
                pos_valid = pos->valid;
                lcd_symbol(LCD_SYM_LOCATION, false);
                lcd_symbol(LCD_SYM_TARGET, true);
            }
            else
            {
                pos = &s->self;
                pos_valid = s->self_valid;
                lcd_symbol(LCD_SYM_LOCATION, true);
                lcd_symbol(LCD_SYM_TARGET, false);
            }

            /* 经度显示在上排（5位） */
            draw_coord_part(1, pos->longitude, true, pos_valid);
        }
        break;

    default:
        break;
    }

    /* 十字准星常亮 */
    lcd_symbol(LCD_SYM_CROSS_T, true);
    lcd_symbol(LCD_SYM_CROSS_L, true);
    lcd_symbol(LCD_SYM_CROSS_B, true);

    draw_battery(s->batt_level);

    lcd_flush();
}
