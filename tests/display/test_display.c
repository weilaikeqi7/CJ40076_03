#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_display.h"
#include "dev_display_map.h"

static int8_t digits[28];
static uint8_t highest[3];
static bool symbols[50];
void lcd_init(void) {}
void lcd_power_on(void) {}
void lcd_fill(void) {}
void lcd_flush(void) {}
void lcd_clear(void) { memset(digits, -1, sizeof(digits)); memset(symbols, 0, sizeof(symbols)); }
void lcd_print_digit(uint8_t pos, int8_t value) { digits[pos] = value; }
void lcd_hiseg_digit(lcd_hiseg_t group, uint8_t value) { highest[group] = value; }
void lcd_symbol(uint8_t sym, bool on) { symbols[sym] = on; }
void lcd_print_uint(uint8_t first, uint8_t count, uint32_t value, bool zero)
{ (void)first; (void)count; (void)value; (void)zero; }

static void expect_dashes(uint8_t first, lcd_hiseg_t group, uint8_t dot)
{
    assert(highest[group] == 0xFFU);
    assert(digits[first] == -2 && digits[first + 1] == -2 && digits[first + 2] == -2);
    assert(digits[first + 3] == -1 && !symbols[dot]);
}

int main(void)
{
    measure_result_t result = {0};
    disp_state_t state = {0};
    state.result = &result;
    state.mode = MEAS_MODE_MULTI;
    state.self_valid = true;
    result.near_valid = true;
    /* 上限 3999.9 米必须完整显示，不能提前判为溢出。 */
    result.near_mm = 3999900U;
    state.self.altitude_m = 3999.0f;
    display_render(&state);
    assert(highest[LCD_HISEG_DIST] == 3 && digits[4] == 9 && digits[5] == 9 && digits[6] == 9);
    assert(digits[7] == 9 && symbols[LCD_SYM_DOT_ROW2]);
    assert(highest[LCD_HISEG_ELEV] == 3 && digits[17] == 9);
    /* 超过特殊千位的表示范围后，距离和高程均显示横杠。 */
    const uint32_t overflow[] = {4000U, 4500U, 12000U};
    for (unsigned i = 0; i < sizeof(overflow) / sizeof(overflow[0]); ++i)
    {
        result.near_mm = overflow[i] * 1000U;
        state.self.altitude_m = (float)overflow[i];
        display_render(&state);
        expect_dashes(4, LCD_HISEG_DIST, LCD_SYM_DOT_ROW2);
        expect_dashes(17, LCD_HISEG_ELEV, LCD_SYM_DOT_BOT);
    }
    result.near_valid = false;
    result.near_mm = 100000U;
    state.self_valid = false;
    state.self.altitude_m = 100.0f;
    display_render(&state);
    expect_dashes(4, LCD_HISEG_DIST, LCD_SYM_DOT_ROW2);
    expect_dashes(17, LCD_HISEG_ELEV, LCD_SYM_DOT_BOT);
    puts("display range regressions passed");
    return 0;
}
