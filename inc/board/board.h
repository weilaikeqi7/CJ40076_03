/**
 * @file board.h
 * @brief CJ40076 V4.0 主板引脚定义与板级资源驱动（N32G4FRHEQ7）
 *
 * 引脚分配（原理图 40076 V4.0）：
 *   PA0  - 电池电压 ADC（20K/10K 分压，VBAT = ADC * 3）
 *   PA1  - 温度检测 ADC（NTC，10K 上拉到 3V3）
 *   PA2/PA3   - USART2 TX/RX -> JY901B 姿态传感器
 *   PA4  - 模式键（低有效）
 *   PA5  - 电源键（低有效）
 *   PA6  - 段码屏 DISP（显示使能，高有效，同时控制背光）
 *   PA7  - 段码屏 EI（串行数据）
 *   PA8  - JY901B 电源开关（高有效）
 *   PA9/PA10  - USART1 TX/RX -> GNSS 模块
 *   PB0/PB1   - UART6 TX/RX -> 测距机（GPIO_RMP3_UART6，UART6_RMP[1:0]=11）
 *   PB3  - 测距机电源开关（高有效）
 *   PB4  - 显示屏电源开关（高有效）
 *   PB6  - 加热丝输出（高有效）
 *   PB12 - 电源保持（高有效，开机后必须置高，置低关机）
 *   PB13 - 段码屏 LP（移位时钟）
 *   PB14 - 段码屏 FR（交流方波 32~96Hz）
 *   PB15 - GNSS 电源开关（高有效）
 */
#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "n32g4fr.h"
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------ 引脚定义 ------------------------------ */

/* 电池电压 ADC */
#define BOARD_VBAT_ADC_PORT GPIOA
#define BOARD_VBAT_ADC_PIN  GPIO_PIN_0
#define BOARD_VBAT_ADC_CH   ADC_CH_0

/* 温度检测 ADC（NTC） */
#define BOARD_NTC_ADC_PORT GPIOA
#define BOARD_NTC_ADC_PIN  GPIO_PIN_1
#define BOARD_NTC_ADC_CH   ADC_CH_1

/* 测距机电源开关，高有效 */
#define BOARD_PWR_RANGER_PORT GPIOB
#define BOARD_PWR_RANGER_PIN  GPIO_PIN_3

/* JY901B 电源开关，高有效 */
#define BOARD_PWR_JY901B_PORT GPIOA
#define BOARD_PWR_JY901B_PIN  GPIO_PIN_8

/* GNSS 电源开关，高有效 */
#define BOARD_PWR_GNSS_PORT GPIOB
#define BOARD_PWR_GNSS_PIN  GPIO_PIN_15

/* 显示屏电源开关，高有效 */
#define BOARD_PWR_LCD_PORT GPIOB
#define BOARD_PWR_LCD_PIN  GPIO_PIN_4

/* 电源保持，高有效（置低后系统掉电关机） */
#define BOARD_PWR_HOLD_PORT GPIOB
#define BOARD_PWR_HOLD_PIN  GPIO_PIN_12

/* 模式键，低有效 */
#define BOARD_KEY_MODE_PORT GPIOA
#define BOARD_KEY_MODE_PIN  GPIO_PIN_4

/* 电源键，低有效 */
#define BOARD_KEY_POWER_PORT GPIOA
#define BOARD_KEY_POWER_PIN GPIO_PIN_5

/* 加热丝输出：PB6 = TIM4_CH1（默认复用，无需重映射），PWM 控制 */
#define BOARD_HEATER_PORT GPIOB
#define BOARD_HEATER_PIN  GPIO_PIN_6
#define BOARD_HEATER_PWM_FREQ_HZ 10000U /* 加热丝 PWM 频率调整为 10kHz（降低低温高内阻电池纹波冲击） */

/* 段码屏接口（详细驱动见 lcd.h） */
#define BOARD_LCD_DISP_PORT GPIOA
#define BOARD_LCD_DISP_PIN  GPIO_PIN_6
#define BOARD_LCD_EI_PORT   GPIOA
#define BOARD_LCD_EI_PIN    GPIO_PIN_7
#define BOARD_LCD_LP_PORT   GPIOB
#define BOARD_LCD_LP_PIN    GPIO_PIN_13
#define BOARD_LCD_FR_PORT   GPIOB
#define BOARD_LCD_FR_PIN    GPIO_PIN_14

/* JY901B -> USART2 */
#define BOARD_JY901B_USART     USART2
#define BOARD_JY901B_TX_PORT   GPIOA
#define BOARD_JY901B_TX_PIN    GPIO_PIN_2
#define BOARD_JY901B_RX_PORT   GPIOA
#define BOARD_JY901B_RX_PIN    GPIO_PIN_3

/* GNSS -> USART1 */
#define BOARD_GNSS_USART     USART1
#define BOARD_GNSS_TX_PORT   GPIOA
#define BOARD_GNSS_TX_PIN    GPIO_PIN_9
#define BOARD_GNSS_RX_PORT   GPIOA
#define BOARD_GNSS_RX_PIN    GPIO_PIN_10

/* 测距机 -> UART6（重映射到 PB0/PB1） */
#define BOARD_RANGER_UART    UART6
#define BOARD_RANGER_TX_PORT GPIOB
#define BOARD_RANGER_TX_PIN  GPIO_PIN_0
#define BOARD_RANGER_RX_PORT GPIOB
#define BOARD_RANGER_RX_PIN  GPIO_PIN_1

/* ------------------------------ GPIO 驱动 ------------------------------ */

/**
 * @brief 电源保持控制。开机后必须立即置 true，否则松开电源键后掉电；
 *        置 false 切断整机电源（软关机）。
 */
void board_power_hold(bool on);

/**
 * @brief 初始化板上所有开关量 GPIO（电源开关、按键、加热丝、电源保持）。
 *        上电默认：所有外设电源关闭，加热丝关闭。
 * @note  本函数内部会把电源保持脚置高（自动保持供电）。
 */
void board_gpio_init(void);

void board_ranger_power(bool on); /* 测距机电源 */
void board_jy901b_power(bool on); /* JY901B 电源 */
void board_gnss_power(bool on);   /* GNSS 电源 */
void board_lcd_power(bool on);    /* 显示屏电源（一般通过 lcd_power_on/off 操作） */

/**
 * @brief 加热丝 PWM 占空比设置。
 * @param permille 占空比千分比 0~1000（0=关，1000=全开）
 * @note  PB6 = TIM4_CH1，频率见 BOARD_HEATER_PWM_FREQ_HZ，高电平有效。
 */
void board_heater_set_duty(uint16_t permille);
void board_heater(bool on);       /* 加热丝全开/全关（等价 duty 1000/0） */

bool board_key_mode_pressed(void);  /* 模式键按下返回 true（低有效） */
bool board_key_power_pressed(void); /* 电源键按下返回 true（低有效） */

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
