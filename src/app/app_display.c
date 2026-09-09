/**
 * @file app_display.c
 * @brief LCD 业务渲染实现
 *
 * TODO（实物确认）：模式图标段号 LCD_ICON_SINGLE / LCD_ICON_CONT 暂无屏厂定义，
 * 暂用 S31/S32 占位，确定后改这两个宏即可。
 */
#include "app_display.h"

#include "app_config.h"
#include "lcd.h"
#include "lcd_map.h"

#include <string.h>

/** 模式图标（TODO 实物确认段号） */
#define LCD_ICON_SINGLE 31 /* S31 */
#define LCD_ICON_CONT   32 /* S32 */

/** 首末目标图标（第二行字母） */
#define LCD_ICON_F 29 /* S29 F */
#define LCD_ICON_E 30 /* S30 E */

/** 中行/顶行符号 */
#define LCD_DEG_TOP LCD_SYM_DEG_TOP /* 顶行 ° */
#define LCD_DEG_MID LCD_SYM_DEG_MID /* 中行 ° */
#define LCD_DEG_BIG LCD_SYM_DEG_BIG /* 大字行 ° */
#define LCD_MIN_SYM LCD_SYM_SLASH1  /* ′ */
#define LCD_SEC_SYM LCD_SYM_SLASH2  /* ″ */
#define LCD_DOT_BIG LCD_SYM_DOT_BIG /* 大字行小数点 */
#define LCD_DOT_MID LCD_SYM_DOT_MID /* 中行小数点 */
#define LCD_DOT_TOP LCD_SYM_DOT_TOP /* 顶行小数点 */
#define LCD_DOT_BOT LCD_SYM_DOT_BOT /* 底行小数点 */

/* ------------------------------ 数字绘制辅助 ------------------------------ */

/** 单 digit 位置显示数字/空白/横杠（value: 0~9, -1 空白, -2 横杠） */
static void put_digit(uint8_t pos, int8_t value)
{
    lcd_print_digit(pos, value);
}

/**
 * @brief 绘制 "特殊段 + 2 位整数 + 1 位小数" 字段（顶行航向 XXX.X°）。
 *        布局：[hiseg=百位] d(十位) d+1(个位) . d+2(小数位)
 */
static void draw_x3_1(lcd_hiseg_t hiseg, uint8_t first_digit, uint8_t dot_sym, uint32_t value_x1,
                      bool valid)
{
    uint32_t int_part = value_x1 / 10U; /* 整数部分 0~359 */
    uint8_t  frac     = (uint8_t)(value_x1 % 10U);
    uint8_t  hundreds;
    uint8_t  tens;

    if (!valid)
    {
        lcd_hiseg_digit(hiseg, 0xFFU);
        put_digit(first_digit, -2);     /* 横杠 */
        put_digit(first_digit + 1U, -2);
        put_digit(first_digit + 2U, -1);
        lcd_symbol(dot_sym, false);
        return;
    }

    if (int_part > 359U)
    {
        int_part %= 360U; /* 航向归一化兜底 */
    }

    hundreds = (uint8_t)(int_part / 100U);
    tens     = (uint8_t)((int_part / 10U) % 10U);

    /* 百位 -> 特殊段（0 无法显示则熄灭） */
    lcd_hiseg_digit(hiseg, hundreds);

    /* 十位：百位为 0 且十位为 0 时留白 */
    put_digit(first_digit, (hundreds == 0U && tens == 0U) ? -1 : (int8_t)tens);
    /* 个位 */
    put_digit(first_digit + 1U, (int8_t)(int_part % 10U));

    lcd_symbol(dot_sym, true);
    put_digit(first_digit + 2U, (int8_t)frac);
}

/**
 * @brief 绘制 "特殊段 + 4 位整数 + 1 位小数" 字段（第二行距离、底行高程）。
 *        布局：[hiseg] d0 d1 d2 . d3
 */
static void draw_x4_1(lcd_hiseg_t hiseg, uint8_t first_digit, uint8_t dot_sym, uint32_t value_x1,
                      bool valid)
{
    uint32_t int_part = value_x1 / 10U; /* 最多 3999（超出丢弃高位） */
    uint8_t  frac     = (uint8_t)(value_x1 % 10U);
    uint8_t  digits[4];
    uint8_t  n = 0U;
    uint8_t  i;
    uint32_t v;

    if (!valid)
    {
        lcd_hiseg_digit(hiseg, 0xFFU);
        for (i = 0U; i < 3U; i++)
        {
            put_digit((uint8_t)(first_digit + i), -2);
        }
        put_digit((uint8_t)(first_digit + 3U), -1);
        lcd_symbol(dot_sym, false);
        return;
    }

    if (int_part > 3999U)
    {
        int_part %= 10000U; /* 超高 4 位丢弃高位 */
    }

    v = int_part;
    do
    {
        digits[n++] = (uint8_t)(v % 10U);
        v /= 10U;
    } while (v > 0U && n < 4U);

    if (n > 3U)
    {
        lcd_hiseg_digit(hiseg, digits[3]); /* 千位 0~3 */
    }
    else
    {
        lcd_hiseg_digit(hiseg, 0xFFU);
    }

    for (i = 0U; i < 3U; i++)
    {
        uint8_t pos = (uint8_t)(first_digit + i);
        uint8_t idx = (uint8_t)(2U - i);
        int8_t  val = -1;

        if (idx < n)
        {
            val = (int8_t)digits[idx];
        }
        put_digit(pos, val);
    }

    lcd_symbol(dot_sym, true);
    put_digit((uint8_t)(first_digit + 3U), (int8_t)frac);
}

/**
 * @brief LCD 俯仰显示映射：±85.00°~±88.00°线性映射为±85.00°~±90.00°。
 *        只改变显示值，不改变坐标和高程解算使用的姿态值。
 */
static int32_t pitch_display_map(int32_t pitch_c01)
{
    const int32_t start_c01 = APP_PIT_DISPLAY_MAP_START_C01;
    const int32_t end_c01   = APP_PIT_DISPLAY_MAP_END_C01;
    const int32_t max_c01   = APP_PIT_MAX_C01;
    bool          negative  = pitch_c01 < 0;
    int32_t       abs_c01   = negative ? -pitch_c01 : pitch_c01;
    int32_t       mapped_c01;

    if (abs_c01 <= start_c01)
    {
        return pitch_c01;
    }
    if (abs_c01 >= end_c01)
    {
        return negative ? -max_c01 : max_c01;
    }

    mapped_c01 = start_c01 +
                 ((abs_c01 - start_c01) * (max_c01 - start_c01) +
                  (end_c01 - start_c01) / 2) /
                     (end_c01 - start_c01);
    return negative ? -mapped_c01 : mapped_c01;
}

/** 中行俯仰：−XX.X°（digits 25 26 . 27，负号 S37，°S35），无效时空白 */
static void draw_pitch(int32_t pitch_c01, bool valid)
{
    uint32_t abs_x1;
    uint32_t int_part;
    uint8_t  frac;

    if (!valid)
    {
        put_digit(25, -1);
        put_digit(26, -1);
        put_digit(27, -1);
        lcd_symbol(LCD_DOT_MID, false);
        lcd_symbol(LCD_SYM_MINUS_MID, false);
        lcd_symbol(LCD_DEG_MID, false);
        lcd_symbol(LCD_SYM_MID_P, false);
        return;
    }

    pitch_c01 = pitch_display_map(pitch_c01);
    abs_x1   = (uint32_t)((pitch_c01 < 0) ? -pitch_c01 : pitch_c01) / 10U; /* 0.1° */
    int_part = abs_x1 / 10U;
    frac     = (uint8_t)(abs_x1 % 10U);

    put_digit(25, int_part >= 10U ? (int8_t)(int_part / 10U) : -1);
    put_digit(26, (int8_t)(int_part % 10U));
    lcd_symbol(LCD_DOT_MID, true);
    put_digit(27, (int8_t)frac);

    lcd_symbol(LCD_SYM_MINUS_MID, pitch_c01 < 0);
    lcd_symbol(LCD_DEG_MID, true);
    lcd_symbol(LCD_SYM_MID_P, true);
}

/**
 * @brief 大字行坐标：DDD° MM′ SS.ss″（digits 8~16）。
 * @param deg 坐标（度，带符号）
 * @param is_lon true=经度 false=纬度（半球指示用 WSEN 字母）
 */
static void draw_coord(double deg, bool is_lon, bool valid)
{
    uint32_t deg_i, min_i, sec_x100;
    double   abs_val;

    /* 清半球指示 */
    lcd_symbol(LCD_SYM_WIND_N, false);
    lcd_symbol(LCD_SYM_WIND_S, false);
    lcd_symbol(LCD_SYM_WIND_E, false);
    lcd_symbol(LCD_SYM_WIND_W, false);

    if (!valid)
    {
        uint8_t i;
        for (i = 8U; i <= 16U; i++)
        {
            put_digit(i, -2);
        }
        lcd_symbol(LCD_DEG_BIG, false);
        lcd_symbol(LCD_MIN_SYM, false);
        lcd_symbol(LCD_SEC_SYM, false);
        lcd_symbol(LCD_DOT_BIG, false);
        return;
    }

    abs_val = deg < 0.0 ? -deg : deg;
    deg_i   = (uint32_t)abs_val;
    {
        double rem       = (abs_val - (double)deg_i) * 60.0;
        double sec;
        min_i            = (uint32_t)rem;
        sec              = (rem - (double)min_i) * 60.0;
        sec_x100         = (uint32_t)(sec * 100.0 + 0.5);
        if (sec_x100 >= 6000U) /* 进位：60.00″ -> 1′ */
        {
            sec_x100 = 0U;
            min_i++;
            if (min_i >= 60U)
            {
                min_i = 0U;
                deg_i++;
            }
        }
    }

    put_digit(8, deg_i >= 100U ? (int8_t)((deg_i / 100U) % 10U) : -1);
    put_digit(9, deg_i >= 10U ? (int8_t)((deg_i / 10U) % 10U) : -1);
    put_digit(10, (int8_t)(deg_i % 10U));
    lcd_symbol(LCD_DEG_BIG, true);

    put_digit(11, (int8_t)(min_i / 10U));
    put_digit(12, (int8_t)(min_i % 10U));
    lcd_symbol(LCD_MIN_SYM, true);

    put_digit(13, (int8_t)(sec_x100 / 1000U));
    put_digit(14, (int8_t)((sec_x100 / 100U) % 10U));
    lcd_symbol(LCD_DOT_BIG, true);
    put_digit(15, (int8_t)((sec_x100 / 10U) % 10U));
    put_digit(16, (int8_t)(sec_x100 % 10U));
    lcd_symbol(LCD_SEC_SYM, true);

    /* 半球指示 */
    if (is_lon)
    {
        lcd_symbol(deg >= 0.0 ? LCD_SYM_WIND_E : LCD_SYM_WIND_W, true);
    }
    else
    {
        lcd_symbol(deg >= 0.0 ? LCD_SYM_WIND_N : LCD_SYM_WIND_S, true);
    }
}

/** 8 方向罗盘字母（顶部 ESWNE），heading_c01 无效时全灭 */
static void draw_compass(int32_t heading_c01, bool valid)
{
    static const struct
    {
        uint8_t a;    /* 主字母 */
        uint8_t b;    /* 组合字母（0=无） */
    } dir_map[8] = {
        {LCD_SYM_ROSE_N, 0},             /* N  */
        {LCD_SYM_ROSE_N, LCD_SYM_ROSE_E2}, /* NE */
        {LCD_SYM_ROSE_E2, 0},            /* E  */
        {LCD_SYM_ROSE_S, LCD_SYM_ROSE_E2}, /* SE */
        {LCD_SYM_ROSE_S, 0},             /* S  */
        {LCD_SYM_ROSE_S, LCD_SYM_ROSE_W}, /* SW */
        {LCD_SYM_ROSE_W, 0},             /* W  */
        {LCD_SYM_ROSE_N, LCD_SYM_ROSE_W}, /* NW */
    };
    uint8_t i;
    uint8_t idx;

    lcd_symbol(LCD_SYM_ROSE_E1, false);
    lcd_symbol(LCD_SYM_ROSE_S, false);
    lcd_symbol(LCD_SYM_ROSE_W, false);
    lcd_symbol(LCD_SYM_ROSE_N, false);
    lcd_symbol(LCD_SYM_ROSE_E2, false);

    if (!valid)
    {
        return;
    }

    idx = (uint8_t)(((heading_c01 + 2250) % 36000) / 4500); /* ±22.5° 分扇区 */
    for (i = 0U; i < 2U; i++)
    {
        uint8_t sym = (i == 0U) ? dir_map[idx].a : dir_map[idx].b;
        if (sym != 0U)
        {
            lcd_symbol(sym, true);
        }
    }
}

/** 电池 4 段（框 + 3 格条） */
static void draw_battery(uint8_t level)
{
    lcd_symbol(LCD_SYM_BATTERY, true);
    lcd_symbol(LCD_SYM_BAT_BAR1, level >= 2U);
    lcd_symbol(LCD_SYM_BAT_BAR2, level >= 3U);
    lcd_symbol(LCD_SYM_BAT_BAR3, level >= 4U);
}

/* ------------------------------ 主渲染 ------------------------------ */

void display_init(void)
{
    lcd_init();
    lcd_power_on();
}

void display_render(const disp_state_t* s)
{
    static uint32_t fe_timer;
    static uint32_t ll_timer;
    static bool     fe_show_far;
    static bool     ll_show_lon;

    bool multi_mode = (s->mode == MEAS_MODE_MULTI || s->mode == MEAS_MODE_TEST);
    uint32_t fe_period =
        multi_mode ? APP_DISP_FE_TOGGLE_SLOW_MS : APP_DISP_FE_TOGGLE_FAST_MS;

    if (s->page == DISP_PAGE_FULL_ON)
    {
        lcd_fill();
        lcd_flush();
        return;
    }

    /* 交替计时 */
    fe_timer += APP_DISP_RENDER_MS;
    if (fe_timer >= fe_period)
    {
        fe_timer    = 0U;
        fe_show_far = !fe_show_far;
    }
    ll_timer += APP_DISP_RENDER_MS;
    if (ll_timer >= APP_DISP_LL_TOGGLE_MS)
    {
        ll_timer    = 0U;
        ll_show_lon = !ll_show_lon;
    }

    lcd_clear();

    /* ---------------- 校准页（PIt/HIt/HEr） ---------------- */
    if (s->page != DISP_PAGE_NONE)
    {
        /* 补偿值显示在高程区（绝对值，0.1°） */
        uint32_t abs_x1 = (uint32_t)(s->page_value_c01 < 0 ? -(int32_t)s->page_value_c01
                                                           : (int32_t)s->page_value_c01) / 10U;

        draw_x4_1(LCD_HISEG_ELEV, 17, LCD_DOT_BOT, abs_x1, true);
        lcd_symbol(LCD_SYM_UNIT_H, true);
        lcd_symbol(LCD_SYM_UNIT_M_BOT, true);

        /* 实时生效值回到各自区域 */
        if (s->page == DISP_PAGE_PIT)
        {
            draw_pitch(s->pitch_c01, s->att_valid);
        }
        else
        {
            /* HIt/HEr 页：顶行实时航向 */
            uint32_t h_x1 = (uint32_t)s->heading_c01 / 10U;
            draw_x3_1(LCD_HISEG_HEADING, 1, LCD_DOT_TOP, h_x1, s->att_valid);
            lcd_symbol(LCD_DEG_TOP, s->att_valid);
        }

        draw_battery(s->batt_level);
        lcd_flush();
        return;
    }

    /* ---------------- 正常模式 ---------------- */

    /* 模式图标（测试 = 单次+连续同亮） */
    lcd_symbol(LCD_ICON_SINGLE, s->mode == MEAS_MODE_SINGLE || s->mode == MEAS_MODE_MULTI ||
                                    s->mode == MEAS_MODE_TEST);
    lcd_symbol(LCD_ICON_CONT, s->mode == MEAS_MODE_CONT || s->mode == MEAS_MODE_TEST);

    /* 顶行航向 + 罗盘（仅多功能/测试） */
    if (multi_mode)
    {
        uint32_t h_x1 = (uint32_t)s->heading_c01 / 10U;
        draw_x3_1(LCD_HISEG_HEADING, 1, LCD_DOT_TOP, h_x1, s->att_valid);
        lcd_symbol(LCD_DEG_TOP, s->att_valid);
        draw_compass(s->heading_c01, s->att_valid);
    }

    /* 中行俯仰（仅多功能/测试） */
    if (multi_mode)
    {
        draw_pitch(s->pitch_c01, s->att_valid);
    }

    /* 第二行距离：F/E 交替 */
    {
        bool        show_far  = fe_show_far && s->result->far_valid;
        bool        dist_ok   = !s->measuring && (show_far ? s->result->far_valid : s->result->near_valid);
        uint32_t    dist_mm   = show_far ? s->result->far_mm : s->result->near_mm;
        uint32_t    dist_x1m  = dist_mm / 100U; /* 0.1m */

        draw_x4_1(LCD_HISEG_DIST, 4, LCD_SYM_DOT_ROW2, dist_x1m, dist_ok);
        /* 第二行右侧 M 为玻璃固定图案（常显），无需驱动 */
        lcd_symbol(LCD_ICON_F, dist_ok && !show_far);
        lcd_symbol(LCD_ICON_E, dist_ok && show_far && s->result->far_valid);
    }

    /* 大字行坐标 + 底行高程（仅多功能/测试；单目标时 F/E 不交替） */
    if (multi_mode)
    {
        bool show_far = fe_show_far && s->result->far_valid && s->target_valid;

        if (s->target_valid)
        {
            /* TARGET：目标坐标/高程 */
            const app_geo_point_t* tgt = show_far ? &s->target_far : &s->target_near;
            bool t_ok = tgt->valid;

            lcd_symbol(LCD_SYM_LOCATION, false);
            lcd_symbol(LCD_SYM_TARGET, true);

            if (!ll_show_lon)
            {
                draw_coord(tgt->latitude, false, t_ok);
            }
            else
            {
                draw_coord(tgt->longitude, true, t_ok);
            }

            {
                uint32_t elev_x1 =
                    (uint32_t)((tgt->altitude_m < 0.0f) ? -tgt->altitude_m : tgt->altitude_m) * 10U;
                draw_x4_1(LCD_HISEG_ELEV, 17, LCD_DOT_BOT, elev_x1, t_ok);
            }
        }
        else
        {
            /* LOCAL：本机坐标/高程 */
            lcd_symbol(LCD_SYM_LOCATION, true);
            lcd_symbol(LCD_SYM_TARGET, false);

            if (!ll_show_lon)
            {
                draw_coord(s->self.latitude, false, s->self_valid);
            }
            else
            {
                draw_coord(s->self.longitude, true, s->self_valid);
            }

            {
                uint32_t elev_x1 =
                    (uint32_t)((s->self.altitude_m < 0.0f) ? -s->self.altitude_m
                                                           : s->self.altitude_m) *
                    10U;
                draw_x4_1(LCD_HISEG_ELEV, 17, LCD_DOT_BOT, elev_x1, s->self_valid);
            }
        }
        lcd_symbol(LCD_SYM_UNIT_H, true);
        lcd_symbol(LCD_SYM_UNIT_M_BOT, true);
    }

    /* 底行计数 21~24 */
    {
        uint32_t c = s->count > APP_COUNT_MAX ? APP_COUNT_MAX : s->count;
        lcd_print_uint(21, 4, c, false);
    }

    /* 十字准星常亮（测量中也可作瞄准指示） */
    lcd_symbol(LCD_SYM_CROSSHAIR, true);

    draw_battery(s->batt_level);

    lcd_flush();
}
