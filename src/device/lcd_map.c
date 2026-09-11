/**
 * @file lcd_map.c
 * @brief FO-DM0001 OLED段码屏段码映射表（基于真值表384个SEG）
 *
 * 真值表结构：NO. -> SEG -> ICON
 *   - NO.：序号 0-383
 *   - SEG：SSD1357段号（SA0-SC127）
 *   - ICON：图标名称（数码管段如1A-15G，符号如S1-S37）
 *
 * 注意：双色OLED每个物理段对应多个SEG（红绿通道），本驱动同时点亮所有通道。
 */
#include "lcd_map.h"

#include "lcd.h"

/**
 * 数码管位 -> SEG编号映射表
 * 每行：{A段SEG, B段SEG, C段SEG, D段SEG, E段SEG, F段SEG, G段SEG}
 * SEG编号 = 真值表NO.编号
 */
static const uint16_t lcd_digit_seg_map[LCD_DIGIT_COUNT][7] = {
    /* 数码管1 (上排) */
    {186, 192, 195, 174, 177, 184, 183},  /* 1A=SA62, 1B=SA64, 1C=SA65, 1D=SA58, 1E=SA59, 1F=SB61, 1G=SA61 */
    /* 数码管2 (上排) */
    {210, 214, 217, 197, 201, 207, 204},  /* 2A=SA70, 2B=SB71, 2C=SB72, 2D=SC65, 2E=SA67, 2F=SA69, 2G=SA68 */
    /* 数码管3 (上排) */
    {234, 238, 239, 221, 224, 230, 227},  /* 3A=SA78, 3B=SB79, 3C=SC79, 3D=SC73, 3E=SC74, 3F=SC76, 3G=SC75 */
    /* 数码管4 (上排) */
    {258, 262, 263, 243, 246, 252, 249},  /* 4A=SA86, 4B=SB87, 4C=SC87, 4D=SA81, 4E=SA82, 4F=SA84, 4G=SA83 */
    /* 数码管5 (上排) */
    {280, 284, 287, 268, 271, 275, 274},  /* 5A=SB93, 5B=SC94, 5C=SC95, 5D=SA89, 5E=SA91, 5F=SB91, 5G=SB90 */
    /* 数码管6 (中排) */
    {94, 95, 96, 90, 91, 93, 92},         /* 6A=SB31, 6B=SC31, 6C=SA32, 6D=SA30, 6E=SB30, 6F=SA31, 6G=SC30 */
    /* 数码管7 (中排) */
    {101, 102, 103, 97, 98, 100, 99},     /* 7A=SC33, 7B=SA34, 7C=SB34, 7D=SB32, 7E=SC32, 7F=SB33, 7G=SA33 */
    /* 数码管8 (中排) */
    {108, 109, 110, 104, 105, 107, 106},  /* 8A=SA36, 8B=SB36, 8C=SC36, 8D=SC34, 8E=SA35, 8F=SC35, 8G=SB35 */
    /* 数码管9 (中排) */
    {115, 116, 117, 111, 112, 114, 113},  /* 9A=SB38, 9B=SC38, 9C=SA39, 9D=SA37, 9E=SB37, 9F=SA38, 9G=SC37 */
    /* 数码管10 (中排) */
    {78, 79, 81, 82, 83, 77, 80},         /* 10A=SA26, 10B=SB26, 10C=SA27, 10D=SB27, 10E=SC27, 10F=SC25, 10G=SC26 */
    /* 数码管11 (下排) */
    {67, 68, 69, 63, 64, 66, 65},         /* 11A=SB22, 11B=SC22, 11C=SA23, 11D=SA21, 11E=SB21, 11F=SA22, 11G=SC21 */
    /* 数码管12 (下排) */
    {74, 75, 76, 70, 71, 73, 72},         /* 12A=SC24, 12B=SA25, 12C=SB25, 12D=SB23, 12E=SC23, 12F=SB24, 12G=SA24 */
    /* 数码管13 (下排) */
    {59, 53, 54, 55, 56, 58, 57},         /* 13A=SC19, 13B=SC17, 13C=SA18, 13D=SB18, 13E=SC18, 13F=SB19, 13G=SA19 */
    /* 数码管14 (下排) */
    {52, 46, 47, 48, 49, 51, 50},         /* 14A=SB17, 14B=SB15, 14C=SC15, 14D=SA16, 14E=SB16, 14F=SA17, 14G=SC16 */
    /* 数码管15 (下排) */
    {38, 39, 41, 42, 43, 45, 40},         /* 15A=SC12, 15B=SA13, 15C=SC13, 15D=SA14, 15E=SB14, 15F=SA15, 15G=SB13 */
};

/**
 * 符号 S 编号 -> SEG编号映射表
 * 索引 = S编号 - 1（如S1对应索引0）
 * 每个符号可能对应多个SEG（这里只列主SEG，完整见lcd_symbol函数）
 */
static const uint16_t lcd_symbol_seg_map[LCD_SYMBOL_COUNT] = {
    294,    /* S1  电池格1 -> SA98 */
    295,    /* S2  电池格2 -> SB98 */
    296,    /* S3  电池格3 -> SC98 */
    297,    /* S4  电池框   -> SA99 */
    84,     /* S5  WiFi     -> SA28 */
    150,    /* S6  定位     -> SA50 */
    305,    /* S7  蓝牙     -> SA101 */
    266,    /* S8  上排小数点 -> SC88 */
    331,    /* S9  Y轴(右上) -> SB110 */
    330,    /* S10 km/h    -> SA110 */
    324,    /* S11 mp/h(上) -> SA108 */
    312,    /* S12 mp/h(下) -> SA104 */
    126,    /* S13 指南针1  -> SA42 */
    129,    /* S14 指南针2  -> SA43 */
    118,    /* S15 箭头     -> SB39 */
    150,    /* S16 靶心     -> SA50 (与S6相同) */
    151,    /* S17 十字-上  -> SB50 */
    136,    /* S18 十字-左  -> SB45 */
    154,    /* S19 十字-下  -> SB51 */
    335,    /* S20 闪电     -> SC111 */
    344,    /* S21 三角形A  -> SB114 */
    348,    /* S22 三角形B  -> SB116 */
    347,    /* S23 三角形C  -> SC115 */
    85,     /* S24 弧形     -> SB28 */
    84,     /* S25 中排小数点 -> SA28 (与S5相同) */
    342,    /* S26 Y轴(右下) -> SA114 */
    322,    /* S27 m(中排)  -> SB107 */
    0,      /* S28 三角形I  -> SA0 */
    60,     /* S29 角度符号  -> SA20 */
    44,     /* S30 下排小数点 -> SC14 */
    309,    /* S31 圆圈     -> SA103 */
    343,    /* S32 Y轴(下排) -> SB114 */
    320,    /* S33 m(下排)  -> SC106 */
    306,    /* S34 三角形H  -> SA102 */
    29,     /* S35 云       -> SC9 */
    24,     /* S36 树       -> SA8 */
    20,     /* S37 设置     -> SC6 */
};

/* 7 段字形：bit0=A ... bit6=G */
static const uint8_t lcd_font[] = {
    0x3F, /* 0: A B C D E F */
    0x06, /* 1: B C */
    0x5B, /* 2: A B D E G */
    0x4F, /* 3: A B C D G */
    0x66, /* 4: B C F G */
    0x6D, /* 5: A C D F G */
    0x7D, /* 6: A C D E F G */
    0x07, /* 7: A B C */
    0x7F, /* 8: A B C D E F G */
    0x6F, /* 9: A B C D F G */
};

#define LCD_FONT_BLANK 0x00U
#define LCD_FONT_MINUS 0x40U /* G段 */

void lcd_digit_seg(uint8_t digit, uint8_t seg, bool on)
{
    if (digit < 1U || digit > LCD_DIGIT_COUNT || seg > LCD_SEG_G)
    {
        return;
    }
    lcd_set_seg(lcd_digit_seg_map[digit - 1U][seg], on);
}

void lcd_symbol(uint8_t sym, bool on)
{
    if (sym < 1U || sym > LCD_SYMBOL_COUNT)
    {
        return;
    }
    lcd_set_seg(lcd_symbol_seg_map[sym - 1U], on);
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
        lcd_digit_seg(digit, seg, (pattern & (1U << seg)) != 0U);
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

void lcd_print_x4_1(uint8_t first_digit, uint32_t value_x1, uint8_t dot_sym, bool valid)
{
    uint32_t int_part = value_x1 / 10U; /* 整数部分 0-9999 */
    uint8_t  frac     = (uint8_t)(value_x1 % 10U);
    uint8_t  i;

    if (!valid)
    {
        /* 无效：显示横杠 */
        for (i = 0U; i < 4U; i++)
        {
            lcd_print_digit((uint8_t)(first_digit + i), -2);
        }
        lcd_symbol(dot_sym, false);
        lcd_print_digit((uint8_t)(first_digit + 4U), -1);
        return;
    }

    if (int_part > 9999U)
    {
        int_part %= 10000U; /* 超过4位丢弃高位 */
    }

    /* 显示4位整数（右对齐，左补空） */
    lcd_print_uint(first_digit, 4, int_part, false);

    /* 显示小数点 */
    lcd_symbol(dot_sym, true);

    /* 显示小数位 */
    lcd_print_digit((uint8_t)(first_digit + 4U), (int8_t)frac);
}

void lcd_print_x3_1(uint8_t first_digit, uint32_t value_x1, uint8_t dot_sym, bool valid)
{
    uint32_t int_part = value_x1 / 10U; /* 整数部分 0-999 */
    uint8_t  frac     = (uint8_t)(value_x1 % 10U);
    uint8_t  i;

    if (!valid)
    {
        /* 无效：显示横杠 */
        for (i = 0U; i < 3U; i++)
        {
            lcd_print_digit((uint8_t)(first_digit + i), -2);
        }
        lcd_symbol(dot_sym, false);
        lcd_print_digit((uint8_t)(first_digit + 3U), -1);
        return;
    }

    if (int_part > 999U)
    {
        int_part %= 1000U; /* 超过3位丢弃高位 */
    }

    /* 显示3位整数（右对齐，左补空） */
    lcd_print_uint(first_digit, 3, int_part, false);

    /* 显示小数点 */
    lcd_symbol(dot_sym, true);

    /* 显示小数位 */
    lcd_print_digit((uint8_t)(first_digit + 3U), (int8_t)frac);
}
