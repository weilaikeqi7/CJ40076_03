/**
 * @file lcd_map.c
 * @brief P1237 段码屏段码映射表（依据屏厂真值表 P1237 / 238 段）
 *
 * 说明：真值表中 Y90 与 Y105 均标注为 "6F"，经确认 Y90=6G、Y105=6F。
 */
#include "lcd_map.h"

#include "lcd.h"

/** 数码管位 -> Y 引脚，列序 {A, B, C, D, E, F, G} */
const uint8_t lcd_digit_map[LCD_DIGIT_COUNT][7] = {
    /*  1 */ {138, 137, 111, 110, 109, 139, 140},
    /*  2 */ {130, 129, 115, 114, 113, 131, 112},
    /*  3 */ {127, 126, 119, 118, 117, 128, 120},
    /*  4 */ {108,  84,  82,  81,  80,  79,  83},
    /*  5 */ {107, 106,  88,  87,  86,  85,  89},
    /*  6 */ {104, 103,  93,  92,  91, 105,  90}, /* 已确认 Y90=6G、Y105=6F */
    /*  7 */ {101, 100,  97,  96,  95, 102,  98},
    /*  8 */ {205, 204, 202, 208, 207, 206, 203},
    /*  9 */ {199, 169, 167, 209, 201, 200, 168},
    /* 10 */ {164, 163,   8,   7, 166, 165,   9},
    /* 11 */ {159, 158, 156,  10, 161, 160, 157},
    /* 12 */ {153, 152, 150,  11, 155, 154, 151},
    /* 13 */ { 70,  69,  67,  12,  72,  71,  68},
    /* 14 */ { 64,  60,  58,  13,  66,  65,  59},
    /* 15 */ { 55,  54,  52,  43,  57,  56,  53},
    /* 16 */ { 49,  48,  46,  44,  51,  50,  47},
    /* 17 */ {214, 213, 224, 223, 222, 215, 221},
    /* 18 */ {211, 210, 228, 227, 226, 212, 225},
    /* 19 */ {  5,   4, 232, 231, 230,   6, 229},
    /* 20 */ {  2,   1, 236, 235, 234,   3, 237},
    /* 21 */ { 18,  17,  22,  21,  20,  19,  23},
    /* 22 */ { 15,  14,  26,  25,  24,  16,  27},
    /* 23 */ { 40,  39,  30,  29,  28,  41,  31},
    /* 24 */ { 37,  36,  34,  33,  32,  38,  35},
    /* 25 */ {180, 179, 186, 185, 184, 181, 182},
    /* 26 */ {176, 175, 189, 188, 187, 177, 178},
    /* 27 */ {172, 171, 193, 192, 191, 173, 194},
};

/** 符号 S 编号 -> Y 引脚 */
const uint8_t lcd_symbol_map[LCD_SYMBOL_COUNT] = {
    42,  45,  61,  62,  63,  73,  74,  75,  76,  77,  /* S1 ~S10 */
    78,  94,  99,  116, 121, 122, 123, 124, 125, 132, /* S11~S20 */
    133, 134, 135, 136, 141, 142, 143, 144, 145, 146, /* S21~S30 */
    147, 148, 149, 162, 170, 174, 183, 190, 195, 196, /* S31~S40 */
    197, 198, 216, 217, 218, 219, 220, 233, 238,      /* S41~S49 */
};

/* 7 段字形：bit0=A ... bit6=G */
static const uint8_t lcd_font[] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F, /* 9 */
};

#define LCD_FONT_BLANK 0x00U
#define LCD_FONT_MINUS 0x40U

/* -------------------- 最高位特殊笔画段 -------------------- */
/*
 * 硬件事实（已确认）：特殊段只能显示 1/2/3，无法显示 0 —— 千位为 0 时熄灭。
 *
 * TODO（实物核对笔画顺序）：每组 4 段笔画排列按布局图箭头位置推测，
 * 1/2/3 的字形组合为占位值，点亮实测后修改下面两张表即可。
 */
static const uint8_t lcd_hiseg_strokes[3][4] = {
    [LCD_HISEG_HEADING] = {25, 26, 27, 28},  /* S25 上, S26 左上, S27 左下, S28 下 */
    [LCD_HISEG_DIST]    = {11, 8, 9, 10},    /* S11 上?, S8 左上?, S9 左下?, S10 下? */
    [LCD_HISEG_ELEV]    = {43, 44, 46, 47},  /* S43 上, S44 左上, S46 左下, S47 下 */
};

static const uint8_t lcd_hiseg_font[4] = {
    0x00, /* 0：无法显示，熄灭（调用方也应避免传入 0） */
    0x06, /* 1：左上+左下（占位，实物核对） */
    0x0D, /* 2：上+左下+下（占位，实物核对） */
    0x09, /* 3：上+下（占位，实物核对） */
};

void lcd_hiseg_digit(lcd_hiseg_t group, uint8_t value)
{
    uint8_t i;
    uint8_t pattern = 0U;

    if (value < 4U)
    {
        pattern = lcd_hiseg_font[value];
    }

    for (i = 0U; i < 4U; i++)
    {
        lcd_symbol(lcd_hiseg_strokes[group][i], (pattern & (1U << i)) != 0U);
    }
}

void lcd_digit_seg(uint8_t digit, uint8_t seg, bool on)
{
    if (digit < 1U || digit > LCD_DIGIT_COUNT || seg > LCD_SEG_G)
    {
        return;
    }
    lcd_set_raw(lcd_digit_map[digit - 1U][seg], on);
}

void lcd_symbol(uint8_t sym, bool on)
{
    if (sym < 1U || sym > LCD_SYMBOL_COUNT)
    {
        return;
    }
    lcd_set_raw(lcd_symbol_map[sym - 1U], on);
}

void lcd_print_digit(uint8_t digit, int8_t value)
{
    uint8_t pattern;
    uint8_t seg;

    if (digit < 1U || digit > LCD_DIGIT_COUNT)
    {
        return;
    }

    if (value >= 0 && value <= 9)
    {
        pattern = lcd_font[value];
    }
    else if (value == -2)
    {
        pattern = LCD_FONT_MINUS;
    }
    else
    {
        pattern = LCD_FONT_BLANK;
    }

    for (seg = 0U; seg < 7U; seg++)
    {
        lcd_set_raw(lcd_digit_map[digit - 1U][seg], (pattern & (1U << seg)) != 0U);
    }
}

void lcd_print_uint(uint8_t first_digit, uint8_t count, uint32_t value, bool leading_zero)
{
    uint8_t  buf[10];
    uint8_t  digits = 0U;
    uint8_t  i;
    uint32_t v     = value;
    uint32_t limit = 1U;

    /* 位数可容纳的最大值+1，溢出则显示全 8 */
    for (i = 0U; i < count && i < 10U; i++)
    {
        limit *= 10U;
    }

    if (v >= limit)
    {
        for (i = 0U; i < count; i++)
        {
            lcd_print_digit((uint8_t)(first_digit + i), 8);
        }
        return;
    }

    do
    {
        buf[digits++] = (uint8_t)(v % 10U);
        v /= 10U;
    } while (v > 0U && digits < 10U);

    for (i = 0U; i < count; i++)
    {
        /* 最右一位（编号最大）放个位 */
        uint8_t pos = (uint8_t)(first_digit + count - 1U - i);

        if (i < digits)
        {
            lcd_print_digit(pos, (int8_t)buf[i]);
        }
        else
        {
            lcd_print_digit(pos, leading_zero ? 0 : -1);
        }
    }
}
