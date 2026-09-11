/**
 * @file lcd.c
 * @brief FO-DM0001 OLED段码屏驱动实现（SSD1357，模拟I2C）
 *
 * SSD1357说明：
 *   - 128x128 双色OLED驱动IC
 *   - I2C地址：0x3C（7位地址，写=0x78，读=0x79）
 *   - 每个段对应GDDRAM中的一位
 *   - 双色控制：红色通道 + 绿色通道
 */
#include "lcd.h"

#include "board.h"
#include "misc.h"

#include <string.h>

/* 前向声明 */
static void lcd_delay_us(uint32_t us);

/* I2C引脚定义 */
#define I2C_SCL_PORT BOARD_LCD_I2C_SCL_PORT
#define I2C_SCL_PIN  BOARD_LCD_I2C_SCL_PIN
#define I2C_SDA_PORT BOARD_LCD_I2C_SDA_PORT
#define I2C_SDA_PIN  BOARD_LCD_I2C_SDA_PIN
#define LCD_RST_PORT BOARD_LCD_RST_PORT
#define LCD_RST_PIN  BOARD_LCD_RST_PIN

/* SSD1357 I2C地址（7位地址0x3C） */
#define SSD1357_I2C_ADDR 0x3CU
#define SSD1357_WRITE    (SSD1357_I2C_ADDR << 1)
#define SSD1357_READ     ((SSD1357_I2C_ADDR << 1) | 0x01U)

/* SSD1357命令 */
#define SSD1357_CMD  0x00U
#define SSD1357_DATA 0x40U

/* 显示缓冲区：384位 = 48字节 */
#define LCD_FRAME_BYTES 48U
static uint8_t lcd_frame[LCD_FRAME_BYTES];

/* I2C延时（微秒级） */
static void i2c_delay(void)
{
    uint32_t count = 10U;
    while (count-- > 0U)
    {
        __NOP();
    }
}

/* I2C GPIO操作 */
static void i2c_scl_high(void) { I2C_SCL_PORT->PBSC = I2C_SCL_PIN; }
static void i2c_scl_low(void)  { I2C_SCL_PORT->PBC = I2C_SCL_PIN; }
static void i2c_sda_high(void) { I2C_SDA_PORT->PBSC = I2C_SDA_PIN; }
static void i2c_sda_low(void)  { I2C_SDA_PORT->PBC = I2C_SDA_PIN; }

/* I2C起始信号 */
static void i2c_start(void)
{
    i2c_sda_high();
    i2c_scl_high();
    i2c_delay();
    i2c_sda_low();
    i2c_delay();
    i2c_scl_low();
    i2c_delay();
}

/* I2C停止信号 */
static void i2c_stop(void)
{
    i2c_sda_low();
    i2c_scl_high();
    i2c_delay();
    i2c_sda_high();
    i2c_delay();
}

/* I2C发送一个字节，返回ACK（true=ACK） */
static bool i2c_write_byte(uint8_t data)
{
    uint8_t i;
    bool ack;

    for (i = 0U; i < 8U; i++)
    {
        if ((data & 0x80U) != 0U)
        {
            i2c_sda_high();
        }
        else
        {
            i2c_sda_low();
        }
        i2c_delay();
        i2c_scl_high();
        i2c_delay();
        i2c_scl_low();
        i2c_delay();
        data <<= 1;
    }

    /* 读取ACK */
    i2c_sda_high(); /* 释放SDA */
    i2c_delay();
    i2c_scl_high();
    i2c_delay();
    ack = (I2C_SDA_PORT->PID & I2C_SDA_PIN) == 0U;
    i2c_scl_low();
    i2c_delay();

    return ack;
}

/* SSD1357写命令 */
static void ssd1357_write_cmd(uint8_t cmd)
{
    i2c_start();
    (void)i2c_write_byte(SSD1357_WRITE);
    (void)i2c_write_byte(SSD1357_CMD);
    (void)i2c_write_byte(cmd);
    i2c_stop();
}

/* SSD1357写数据 */
static void ssd1357_write_data(uint8_t data)
{
    i2c_start();
    (void)i2c_write_byte(SSD1357_WRITE);
    (void)i2c_write_byte(SSD1357_DATA);
    (void)i2c_write_byte(data);
    i2c_stop();
}

/* SSD1357初始化序列 */
static void ssd1357_init(void)
{
    /* 复位SSD1357 */
    LCD_RST_PORT->PBC = LCD_RST_PIN;
    lcd_delay_us(10000U); /* 10ms */
    LCD_RST_PORT->PBSC = LCD_RST_PIN;
    lcd_delay_us(10000U);

    /* 初始化命令序列（参考SSD1357 datasheet） */
    ssd1357_write_cmd(0xFDU); /* Set Command Lock */
    ssd1357_write_cmd(0x12U); /* Unlock */

    ssd1357_write_cmd(0xAEU); /* Display Off */

    ssd1357_write_cmd(0xB3U); /* Display Clock Div */
    ssd1357_write_cmd(0xF1U);

    ssd1357_write_cmd(0xCAU); /* Multiplex Ratio */
    ssd1357_write_cmd(0x7FU); /* 1/128 */

    ssd1357_write_cmd(0xA0U); /* Set Re-map */
    ssd1357_write_cmd(0x74U);

    ssd1357_write_cmd(0xA1U); /* Set Display Start Line */
    ssd1357_write_cmd(0x00U);

    ssd1357_write_cmd(0xA2U); /* Set Display Offset */
    ssd1357_write_cmd(0x00U);

    ssd1357_write_cmd(0xABU); /* Function Selection A */
    ssd1357_write_cmd(0x01U); /* Enable internal VDD regulator */

    ssd1357_write_cmd(0xB4U); /* Phase Length */
    ssd1357_write_cmd(0xA0U);
    ssd1357_write_cmd(0xB5U);
    ssd1357_write_cmd(0x55U);

    ssd1357_write_cmd(0xC1U); /* Set Contrast Current */
    ssd1357_write_cmd(0xC8U);

    ssd1357_write_cmd(0xC7U); /* Master Contrast Current Control */
    ssd1357_write_cmd(0x0FU);

    ssd1357_write_cmd(0xB1U); /* Set Pre-charge voltage */
    ssd1357_write_cmd(0x32U);

    ssd1357_write_cmd(0xB2U); /* Display Enhancement */
    ssd1357_write_cmd(0xA4U);
    ssd1357_write_cmd(0x00U);
    ssd1357_write_cmd(0x00U);

    ssd1357_write_cmd(0xBBU); /* Set Second Pre-charge voltage */
    ssd1357_write_cmd(0x17U);

    ssd1357_write_cmd(0xB6U); /* Set Second Pre-charge period */
    ssd1357_write_cmd(0x01U);

    ssd1357_write_cmd(0xBEU); /* Set VCOMH */
    ssd1357_write_cmd(0x05U);

    ssd1357_write_cmd(0xA4U); /* Normal Display */

    ssd1357_write_cmd(0xAFU); /* Display On */
}

/* 微秒级延时 */
static void lcd_delay_us(uint32_t us)
{
    uint32_t count = us * (SystemCoreClock / 1000000U) / 4U;
    while (count-- > 0U)
    {
        __NOP();
    }
}

void lcd_init(void)
{
    GPIO_InitType gpio_init;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_GPIOC, ENABLE);

    /* 配置I2C引脚为开漏输出 */
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = I2C_SCL_PIN | I2C_SDA_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_OD;
    gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitPeripheral(GPIOC, &gpio_init);

    /* 配置复位引脚为推挽输出 */
    gpio_init.Pin       = LCD_RST_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitPeripheral(GPIOA, &gpio_init);

    /* 空闲电平 */
    i2c_scl_high();
    i2c_sda_high();
    LCD_RST_PORT->PBSC = LCD_RST_PIN;

    lcd_clear();
}

void lcd_power_on(void)
{
    board_lcd_power(true);
    lcd_delay_us(20000U); /* 等屏电源稳定20ms */

    ssd1357_init();

    lcd_clear();
    lcd_flush();
}

void lcd_power_off(void)
{
    ssd1357_write_cmd(0xAEU); /* Display Off */
    lcd_delay_us(1000U);
    board_lcd_power(false);
}

void lcd_clear(void)
{
    memset(lcd_frame, 0x00, sizeof(lcd_frame));
}

void lcd_fill(void)
{
    memset(lcd_frame, 0xFF, sizeof(lcd_frame));
}

void lcd_set_seg(uint16_t seg, bool on)
{
    uint16_t byte_idx;
    uint8_t  bit_idx;

    if (seg >= 384U)
    {
        return;
    }

    byte_idx = seg / 8U;
    bit_idx  = (uint8_t)(seg % 8U);

    if (on)
    {
        lcd_frame[byte_idx] |= (uint8_t)(1U << bit_idx);
    }
    else
    {
        lcd_frame[byte_idx] &= (uint8_t)~(1U << bit_idx);
    }
}

void lcd_flush(void)
{
    uint16_t i;

    /* 设置列地址（SEG） */
    ssd1357_write_cmd(0x15U); /* Set Column Address */
    ssd1357_write_cmd(0x00U); /* Start = 0 */
    ssd1357_write_cmd(0x7FU); /* End = 127 */

    /* 设置行地址（COM） */
    ssd1357_write_cmd(0x75U); /* Set Row Address */
    ssd1357_write_cmd(0x00U); /* Start = 0 */
    ssd1357_write_cmd(0x7FU); /* End = 127 */

    /* 写显示数据 */
    i2c_start();
    (void)i2c_write_byte(SSD1357_WRITE);
    (void)i2c_write_byte(SSD1357_DATA);

    for (i = 0U; i < LCD_FRAME_BYTES; i++)
    {
        (void)i2c_write_byte(lcd_frame[i]);
    }

    i2c_stop();
}

uint8_t* lcd_frame_buffer(void)
{
    return lcd_frame;
}
