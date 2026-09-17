/**
 * @file lcd_map.h
 * @brief P1237 段码屏段码映射：数码管位/符号 -> 屏引脚 Y 编号
 *
 * 屏面布局（按 P1237 布局图，符号编号 = 真值表 S 编号）：
 *   顶部罗盘：字母 E S W N E = S24 S23 S22 S21 S20
 *   顶行：    修饰图形（由 S25~S28 四段组成），数码管 1 2 3，
 *             小数点 S14，° S19；电池框 S15，电量格 S17(左) S18(中) S16(右)
 *   第二行：  字母 E(S30) F(S29) L(S31) P(S32)，修饰图形（S8~S11 四段），
 *             数码管 4 5 6 7，小数点 S12，M 右侧 S13
 *   中间行：  负号 S37，P(S36)，数码管 25 26 27，° S35，小数点 S38
 *   中心：    十字准星 S7
 *   大字行：  字母 W(S41) S(S42) E(S40) N(S39)，定位图标 S6，靶心图标 S5，
 *             F(S4) E(S3)，数码管 8 9 10，° S34，11 12，"/" S33，
 *             13 14，小数点 S1，15 16，"//" S2
 *   底行：    H(S45)，修饰图形（S43/S44/S46/S47 四段），数码管 17 18 19 20，
 *             小数点 S48，M(S49)；右下数码管 21 22 23 24
 *
 * 注：S8~S11、S25~S28、S43/S44/S46/S47 为各行首位的修饰图形笔画段，
 *     确切形状以实物点亮为准。
 */
#ifndef LCD_MAP_H
#define LCD_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 数码管总数（屏上编号 1~27） */
#define LCD_DIGIT_COUNT 27U
/** 符号段总数（S1~S49） */
#define LCD_SYMBOL_COUNT 49U

/** 数码管段索引 */
enum
{
    LCD_SEG_A = 0,
    LCD_SEG_B,
    LCD_SEG_C,
    LCD_SEG_D,
    LCD_SEG_E,
    LCD_SEG_F,
    LCD_SEG_G
};

/** 符号段语义（编号 = 真值表 S 编号） */
enum
{
    LCD_SYM_DOT_BIG    = 1,  /* S1  大字行小数点（14、15 之间） */
    LCD_SYM_SLASH2     = 2,  /* S2  大字行 "//" */
    LCD_SYM_UNIT_E_BIG = 3,  /* S3  大字行字母 E */
    LCD_SYM_UNIT_F_BIG = 4,  /* S4  大字行字母 F */
    LCD_SYM_TARGET     = 5,  /* S5  靶心/回零图标 */
    LCD_SYM_LOCATION   = 6,  /* S6  定位（图钉）图标 */
    LCD_SYM_CROSSHAIR  = 7,  /* S7  中心十字准星 */
    LCD_SYM_DOT_ROW2   = 12, /* S12 第二行小数点 */
    LCD_SYM_AFTER_M2   = 13, /* S13 第二行 M 右侧标记 */
    LCD_SYM_DOT_TOP    = 14, /* S14 顶行小数点 */
    LCD_SYM_BATTERY    = 15, /* S15 电池框 */
    LCD_SYM_BAT_BAR3   = 16, /* S16 电池电量格（右） */
    LCD_SYM_BAT_BAR1   = 17, /* S17 电池电量格（左） */
    LCD_SYM_BAT_BAR2   = 18, /* S18 电池电量格（中） */
    LCD_SYM_DEG_TOP    = 19, /* S19 顶行 ° */
    LCD_SYM_ROSE_E2    = 20, /* S20 顶部罗盘字母 E（右） */
    LCD_SYM_ROSE_N     = 21, /* S21 顶部罗盘字母 N */
    LCD_SYM_ROSE_W     = 22, /* S22 顶部罗盘字母 W */
    LCD_SYM_ROSE_S     = 23, /* S23 顶部罗盘字母 S */
    LCD_SYM_ROSE_E1    = 24, /* S24 顶部罗盘字母 E（左） */
    LCD_SYM_UNIT_F2    = 29, /* S29 第二行字母 F */
    LCD_SYM_UNIT_E2    = 30, /* S30 第二行字母 E */
    LCD_SYM_UNIT_L     = 31, /* S31 第二行字母 L */
    LCD_SYM_UNIT_P2    = 32, /* S32 第二行字母 P */
    LCD_SYM_SLASH1     = 33, /* S33 大字行 "/"（12、13 之间） */
    LCD_SYM_DEG_BIG    = 34, /* S34 大字行 ° */
    LCD_SYM_DEG_MID    = 35, /* S35 中间行 ° */
    LCD_SYM_MID_P      = 36, /* S36 中间行字母 P */
    LCD_SYM_MINUS_MID  = 37, /* S37 中间行负号 "−" */
    LCD_SYM_DOT_MID    = 38, /* S38 中间行小数点（27 下方） */
    LCD_SYM_WIND_N     = 39, /* S39 大字行罗盘字母 N */
    LCD_SYM_WIND_E     = 40, /* S40 大字行罗盘字母 E */
    LCD_SYM_WIND_W     = 41, /* S41 大字行罗盘字母 W */
    LCD_SYM_WIND_S     = 42, /* S42 大字行罗盘字母 S */
    LCD_SYM_UNIT_H     = 45, /* S45 底行字母 H */
    LCD_SYM_DOT_BOT    = 48, /* S48 底行小数点（19、20 之间） */
    LCD_SYM_UNIT_M_BOT = 49  /* S49 底行字母 M */
};

/**
 * @brief 点亮/熄灭某一位数码管的一段。
 * @param digit 数码管编号 1~27
 * @param seg   段 LCD_SEG_A~LCD_SEG_G
 */
void lcd_digit_seg(uint8_t digit, uint8_t seg, bool on);

/**
 * @brief 点亮/熄灭符号段。
 * @param sym 符号编号 1~49（真值表 S 编号，可用上方 LCD_SYM_xxx 枚举）
 */
void lcd_symbol(uint8_t sym, bool on);

/**
 * @brief 一位数码管显示数字。
 * @param digit 数码管编号 1~27
 * @param value 0~9 显示数字；-1 全灭；-2 显示负号 "-"
 */
void lcd_print_digit(uint8_t digit, int8_t value);

/**
 * @brief 从指定数码管开始显示无符号数（靠右对齐，左侧补空或补 0）。
 * @param first_digit   最左边一位的数码管编号
 * @param count         占用位数（编号连续递增）
 * @param value         数值（超出位数时显示全 8）
 * @param leading_zero  true=左侧补 0，false=左侧补空
 */
void lcd_print_uint(uint8_t first_digit, uint8_t count, uint32_t value, bool leading_zero);

/** 数码管位 -> Y 引脚映射表：[位号-1][段 A~G] */
extern const uint8_t lcd_digit_map[LCD_DIGIT_COUNT][7];

/** 符号 S 编号 -> Y 引脚映射表：[S编号-1] */
extern const uint8_t lcd_symbol_map[LCD_SYMBOL_COUNT];

/* -------------------- 最高位特殊笔画段（4 段构成，只需显示 0~3） -------------------- */

typedef enum
{
    LCD_HISEG_HEADING = 0, /* 顶行首位（航向百位），笔画 S25~S28 */
    LCD_HISEG_DIST,        /* 第二行首位（距离千位），笔画 S8~S11 */
    LCD_HISEG_ELEV,        /* 底行首位（高程千位），笔画 S43/S44/S46/S47 */
} lcd_hiseg_t;

/**
 * @brief 最高位特殊段显示 1~3（value=0 或 0xFF 熄灭——该段物理上无法显示 0）。
 * @note  4 段笔画的排列与 1/2/3 字形表为推测值（见 lcd_map.c 注释），
 *        实物点亮核对后只需修改 lcd_map.c 中两张表。
 */
void lcd_hiseg_digit(lcd_hiseg_t group, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* LCD_MAP_H */
