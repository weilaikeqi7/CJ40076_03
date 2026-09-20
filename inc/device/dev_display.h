/**
 * @file dev_display.h
 * @brief P1237 段码屏底层驱动（238 段，四线串行接口）
 *
 * 接口信号（主板 P3 -> 显示板 H1）：
 *   DISP - PA6  显示使能（高有效；同时驱动显示板背光 LED）
 *   EI   - PA7  串行数据
 *   LP   - PB13 移位时钟（每 bit 一个正脉冲）
 *   FR   - PB14 交流方波（TIM3 中断驱动，实际频率由 BSP_TIMER_FR_FREQ_HZ 配置）
 *
 * 数据格式（参考 DEMO240.C）：
 *   共 240bit = 30 字节，每字节 LSB 先发；
 *   第 1 字节对应屏引脚 Y240..Y233，最后 1 字节对应 Y8..Y1；
 *   位 = 1 段亮，位 = 0 段灭。Y240 为 COMMON 脚（恒 0），Y239 空脚。
 *
 * 注意：关屏顺序必须先拉低 DISP（关闭显示驱动输出），再断 LCD_VCC，
 *       否则会损伤液晶屏（DEMO240.C 原厂注释）。
 */
#ifndef LCD_H
#define LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 帧缓冲字节数（240 bit） */
#define LCD_FRAME_BYTES 30U
/** 屏引脚总数（Y1..Y240） */
#define LCD_PIN_COUNT 240U
/** FR 交流方波频率（Hz），规格 32~96Hz */
#define LCD_FR_FREQ_HZ 62U

/**
 * @brief 初始化 LCD 接口 GPIO 并清空帧缓冲，不上电、不启动 FR。
 * @note 屏幕上电前须完成 bsp_power_init() 和 lcd_init()；FR 在 lcd_power_on() 中启动。
 */
void lcd_init(void);

/**
 * @brief 屏上电并开显示：LCD_VCC -> 延时 -> 清屏 -> 启动 FR -> DISP=1（含背光）
 */
void lcd_power_on(void);

/**
 * @brief 关显示并断电：DISP=0 -> 停 FR -> 断 LCD_VCC（顺序不可颠倒）
 */
void lcd_power_off(void);

/** 清帧缓冲（全灭，不刷新到屏） */
void lcd_clear(void);

/** 帧缓冲全置位（全亮，不刷新到屏） */
void lcd_fill(void);

/**
 * @brief 设置单个屏引脚对应段（修改帧缓冲，需 lcd_flush 生效）。
 * @param y_pin 屏引脚号 1..240（Y239/Y240 无效，自动忽略）
 * @param on    true=亮，false=灭
 */
void lcd_set_raw(uint8_t y_pin, bool on);

/** 阻塞移出 30 字节；240 位各含两次约 2us 延时，另有 GPIO/循环及中断开销。
 *  须先初始化；发送期间不得由其他任务改帧缓冲或切换屏电源。 */
void lcd_flush(void);

/** 帧缓冲直接访问（高级用法），随后调用 lcd_flush 生效 */
uint8_t* lcd_frame_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_H */
