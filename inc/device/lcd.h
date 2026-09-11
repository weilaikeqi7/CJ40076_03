/**
 * @file lcd.h
 * @brief FO-DM0001-RG-A0 OLED段码屏驱动（SSD1357驱动IC，I2C接口）
 *
 * 硬件参数：
 *   - 142个段码Icon（15位数码管 + 37个符号）
 *   - 驱动IC：SSD1357（双色OLED，红+绿）
 *   - 接口：I2C（使用PC4=SCL, PC5=SDA）
 *   - 复位：PA6
 *
 * 显示资源：
 *   - 数码管1-5：上排（4位整数+1位小数），小数点S8
 *   - 数码管6-10：中排（4位整数+1位小数），小数点S25
 *   - 数码管11-15：下排（4位整数+1位小数），小数点S30
 *   - 符号S1-S37：电池、WiFi、定位、蓝牙、单位等图标
 *
 * SSD1357说明：
 *   - 128 SEG x 128 COM 双色OLED驱动
 *   - 每个段对应2个SEG（红色通道 + 绿色通道）
 *   - I2C地址：0x3C（7位）或 0x3D，取决于硬件连接
 */
#ifndef LCD_H
#define LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 数码管总数 */
#define LCD_DIGIT_COUNT 15U
/** 符号段总数 */
#define LCD_SYMBOL_COUNT 37U

/**
 * @brief 初始化LCD接口（I2C GPIO配置，不上电）
 */
void lcd_init(void);

/**
 * @brief 屏上电并初始化SSD1357
 */
void lcd_power_on(void);

/**
 * @brief 关显示并断电
 */
void lcd_power_off(void);

/**
 * @brief 清除显示缓冲（全灭）
 */
void lcd_clear(void);

/**
 * @brief 填充显示缓冲（全亮，用于测试/校准）
 */
void lcd_fill(void);

/**
 * @brief 设置单个段码（修改缓冲区，需lcd_flush生效）
 * @param seg 段码编号（0-383，对应真值表NO.编号）
 * @param on true=点亮，false=熄灭
 */
void lcd_set_seg(uint16_t seg, bool on);

/**
 * @brief 刷新显示（将缓冲区写入SSD1357）
 */
void lcd_flush(void);

/**
 * @brief 获取显示缓冲区指针（高级用法）
 * @return 指向显示缓冲区的指针（384位 = 48字节）
 */
uint8_t* lcd_frame_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_H */
