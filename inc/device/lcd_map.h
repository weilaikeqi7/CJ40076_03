/**
 * @file lcd_map.h
 * @brief FO-DM0001 OLED段码屏段码映射：数码管位/符号 -> 真值表SEG编号
 *
 * 屏面布局（按 FO-DM0001 规格书）：
 *   顶部：    电池格 S1/S2/S3，电池框 S4，WiFi S5，定位 S6，蓝牙 S7
 *   上排：    数码管 1 2 3 4 5，小数点 S8
 *   右上：    Y(S9)，km/h(S10)，mp/h(S11/S12)
 *   左侧：    指南针 S13/S14，箭头 S15
 *   中心：    靶心 S16，十字准星 S17-S19
 *   右侧：    闪电 S20，三角形ABC S21/S22/S23
 *   左下：    弧形 S24，角度符号 S29
 *   中排：    数码管 6 7 8 9 10，小数点 S25
 *   右下：    Y(S26)，m(S27)，三角形I(S28)，S31-S34
 *   下排：    数码管 11 12 13 14 15，小数点 S30
 *   底部：    云 S35，树 S36，设置 S37
 *
 * 真值表说明：
 *   - 共384个SEG（NO.0-383），每个段对应SSD1357的一个SEG引脚
 *   - 数码管段标记：数字+字母（如1A=数码管1的A段）
 *   - 符号标记：S+数字（如S28=符号28）
 *   - 每个物理段可能对应多个SEG（红绿双色通道）
 */
#ifndef LCD_MAP_H
#define LCD_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 数码管总数（屏上编号 1~15） */
#define LCD_DIGIT_COUNT 15U
/** 符号段总数（S1~S37） */
#define LCD_SYMBOL_COUNT 37U

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
    LCD_SYM_BAT_BAR1   = 1,  /* S1  电池电量格（左） */
    LCD_SYM_BAT_BAR2   = 2,  /* S2  电池电量格（中） */
    LCD_SYM_BAT_BAR3   = 3,  /* S3  电池电量格（右） */
    LCD_SYM_BATTERY    = 4,  /* S4  电池框 */
    LCD_SYM_WIFI       = 5,  /* S5  WiFi图标（借用为F/近目标） */
    LCD_SYM_LOCATION   = 6,  /* S6  定位图标 */
    LCD_SYM_BT         = 7,  /* S7  蓝牙图标（借用为E/远目标） */
    LCD_SYM_DOT_TOP    = 8,  /* S8  上排小数点 */
    LCD_SYM_UNIT_Y1    = 9,  /* S9  Y轴符号（右上） */
    LCD_SYM_UNIT_KMH   = 10, /* S10 km/h */
    LCD_SYM_UNIT_MPH1  = 11, /* S11 mp/h（上） */
    LCD_SYM_UNIT_MPH2  = 12, /* S12 mp/h（下） */
    LCD_SYM_COMPASS1   = 13, /* S13 指南针1 */
    LCD_SYM_COMPASS2   = 14, /* S14 指南针2 */
    LCD_SYM_ARROW      = 15, /* S15 箭头 */
    LCD_SYM_TARGET     = 16, /* S16 靶心/回零图标 */
    LCD_SYM_CROSS_T    = 17, /* S17 十字准星-上 */
    LCD_SYM_CROSS_L    = 18, /* S18 十字准星-左 */
    LCD_SYM_CROSS_B    = 19, /* S19 十字准星-下 */
    LCD_SYM_LIGHTNING  = 20, /* S20 闪电 */
    LCD_SYM_TRI_A      = 21, /* S21 三角形A */
    LCD_SYM_TRI_B      = 22, /* S22 三角形B */
    LCD_SYM_TRI_C      = 23, /* S23 三角形C */
    LCD_SYM_ARC        = 24, /* S24 弧形/彩虹 */
    LCD_SYM_DOT_MID    = 25, /* S25 中排小数点 */
    LCD_SYM_UNIT_Y2    = 26, /* S26 Y轴符号（右下） */
    LCD_SYM_UNIT_M1    = 27, /* S27 m单位（中排右） */
    LCD_SYM_TRI_I      = 28, /* S28 三角形I */
    LCD_SYM_ANGLE      = 29, /* S29 角度符号 */
    LCD_SYM_DOT_BOT    = 30, /* S30 下排小数点 */
    LCD_SYM_CIRCLE     = 31, /* S31 圆圈 */
    LCD_SYM_UNIT_Y3    = 32, /* S32 Y轴符号（下排右） */
    LCD_SYM_UNIT_M2    = 33, /* S33 m单位（下排右） */
    LCD_SYM_TRI_H      = 34, /* S34 三角形H */
    LCD_SYM_CLOUD      = 35, /* S35 云 */
    LCD_SYM_TREE       = 36, /* S36 树 */
    LCD_SYM_SETTING    = 37  /* S37 设置/眼镜 */
};

/**
 * @brief 点亮/熄灭某一位数码管的一段。
 * @param digit 数码管编号 1~15
 * @param seg   段 LCD_SEG_A~LCD_SEG_G
 */
void lcd_digit_seg(uint8_t digit, uint8_t seg, bool on);

/**
 * @brief 点亮/熄灭符号段。
 * @param sym 符号编号 1~37（真值表 S 编号，可用上方 LCD_SYM_xxx 枚举）
 */
void lcd_symbol(uint8_t sym, bool on);

/**
 * @brief 一位数码管显示数字。
 * @param digit 数码管编号 1~15
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

/**
 * @brief 显示带1位小数的数值（4位整数+1位小数，共5位数码管）
 * @param first_digit 最左边一位的数码管编号
 * @param value_x1   数值（放大10倍，如1234表示123.4）
 * @param dot_sym    小数点符号编号（S8/S25/S30）
 * @param valid      数据有效性（false显示横杠）
 */
void lcd_print_x4_1(uint8_t first_digit, uint32_t value_x1, uint8_t dot_sym, bool valid);

/**
 * @brief 显示带1位小数的数值（3位整数+1位小数，共4位数码管）
 * @param first_digit 最左边一位的数码管编号
 * @param value_x1   数值（放大10倍，如1234表示123.4）
 * @param dot_sym    小数点符号编号（S8/S25/S30）
 * @param valid      数据有效性（false显示横杠）
 */
void lcd_print_x3_1(uint8_t first_digit, uint32_t value_x1, uint8_t dot_sym, bool valid);

#ifdef __cplusplus
}
#endif

#endif /* LCD_MAP_H */
