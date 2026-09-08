/**
 * @file gnss.c
 * @brief BV-220 GNSS 模块驱动实现（NMEA0183 V4.10 行解析 + 异或校验）
 */
#include "gnss.h"

#include "board.h"
#include "board_uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

#define GNSS_UART BOARD_UART_GNSS
#define GNSS_BAUD 115200U

#define NMEA_LINE_MAX 96U

static gnss_data_t gnss_data;

/* ------------------------------ 字段解析辅助 ------------------------------ */

/** ddmm.mmmmm / dddmm.mmmmm -> 度（不处理符号，半球方向由调用方处理） */
static double nmea_coord_to_deg(const char* str)
{
    double  raw;
    double  deg_int;
    double  minutes;
    int     deg_digits = 2; /* 纬度 2 位度数 */
    double  scale      = 1.0;
    double  value      = 0.0;
    const char* p      = str;
    int     frac_seen  = 0;
    double  frac_div   = 1.0;

    /* 手工解析避免 locale 问题 */
    while (*p != '\0' && *p != ',')
    {
        if (*p == '.')
        {
            frac_seen = 1;
        }
        else if (*p >= '0' && *p <= '9')
        {
            value = value * 10.0 + (double)(*p - '0');
            if (frac_seen)
            {
                frac_div *= 10.0;
            }
        }
        p++;
    }
    raw = value / frac_div;

    /* 判断经度（3 位度数）还是纬度（2 位）：度数部分 = 整数部分去掉最后 2 位分 */
    deg_int = (double)(int)(raw / 100.0);
    minutes = raw - deg_int * 100.0;
    (void)deg_digits;
    (void)scale;

    return deg_int + minutes / 60.0;
}

static float nmea_atof(const char* str)
{
    if (str == NULL || *str == '\0')
    {
        return 0.0f;
    }
    return (float)strtod(str, NULL);
}

static uint32_t nmea_atou(const char* str)
{
    uint32_t v = 0U;

    while (*str >= '0' && *str <= '9')
    {
        v = v * 10U + (uint32_t)(*str - '0');
        str++;
    }
    return v;
}

static uint8_t nmea_hex_digit(uint8_t c)
{
    if (c >= '0' && c <= '9')
    {
        return (uint8_t)(c - '0');
    }
    c |= 0x20U; /* 大写转小写 */
    if (c >= 'a' && c <= 'f')
    {
        return (uint8_t)(c - 'a' + 10U);
    }
    return 0xFFU; /* 非法字符 */
}

/** 校验：$ 与 * 之间所有字符异或 == * 后两位十六进制 */
static bool nmea_checksum_ok(const char* line, int len)
{
    uint8_t     xor_sum = 0U;
    int         i;
    const char* star = NULL;

    if (len < 4 || line[0] != '$')
    {
        return false;
    }

    for (i = 1; i < len; i++)
    {
        if (line[i] == '*')
        {
            star = &line[i];
            break;
        }
        xor_sum ^= (uint8_t)line[i];
    }

    if (star == NULL || (star + 3) > (line + len))
    {
        return false;
    }

    {
        uint8_t hi = nmea_hex_digit((uint8_t)star[1]);
        uint8_t lo = nmea_hex_digit((uint8_t)star[2]);
        uint8_t ref;

        if (hi > 0x0FU || lo > 0x0FU)
        {
            return false;
        }
        ref = (uint8_t)((hi << 4) | lo);

        return ref == xor_sum;
    }
}

/* 按逗号切分（就地修改），返回字段数 */
static int nmea_split(char* line, char* fields[], int max_fields)
{
    int n = 0;

    fields[n++] = line;
    while (*line != '\0' && n < max_fields)
    {
        if (*line == ',' || *line == '*')
        {
            char stop = *line;
            *line = '\0';
            fields[n++] = line + 1;
            if (stop == '*')
            {
                break; /* 校验字段后不继续 */
            }
        }
        line++;
    }
    return n;
}

/* ------------------------------ 语句处理 ------------------------------ */

/** $xxRMC,UTC,STATUS,LAT,NS,LON,EW,SPEED,COURSE,DATE,MAG,MAGDIR,POSMODE,NAVST*cs */
static void gnss_handle_rmc(char* line)
{
    char*  fields[16];
    int    n = nmea_split(line, fields, 16);
    double lat;
    double lon;

    if (n < 10)
    {
        return;
    }

    /* fields[0]="$xxRMC" */
    gnss_data.valid = (fields[2][0] == 'A');

    if (strlen(fields[1]) >= 6U)
    {
        gnss_data.utc_hour = (uint8_t)((fields[1][0] - '0') * 10 + (fields[1][1] - '0'));
        gnss_data.utc_min  = (uint8_t)((fields[1][2] - '0') * 10 + (fields[1][3] - '0'));
        gnss_data.utc_sec  = (uint8_t)((fields[1][4] - '0') * 10 + (fields[1][5] - '0'));
    }
    if (strlen(fields[9]) >= 6U)
    {
        gnss_data.date_day   = (uint8_t)((fields[9][0] - '0') * 10 + (fields[9][1] - '0'));
        gnss_data.date_month = (uint8_t)((fields[9][2] - '0') * 10 + (fields[9][3] - '0'));
        gnss_data.date_year  = (uint8_t)((fields[9][4] - '0') * 10 + (fields[9][5] - '0'));
    }

    if (gnss_data.valid && fields[3][0] != '\0' && fields[5][0] != '\0')
    {
        lat = nmea_coord_to_deg(fields[3]);
        lon = nmea_coord_to_deg(fields[5]);
        if (fields[4][0] == 'S')
        {
            lat = -lat;
        }
        if (fields[6][0] == 'W')
        {
            lon = -lon;
        }
        gnss_data.latitude  = lat;
        gnss_data.longitude = lon;
    }

    gnss_data.speed_knots = nmea_atof(fields[7]);
    gnss_data.speed_kmh   = gnss_data.speed_knots * 1.852f;
    gnss_data.course_deg  = nmea_atof(fields[8]);
    if (n > 12 && fields[12][0] != '\0')
    {
        gnss_data.pos_mode = fields[12][0];
    }

    gnss_data.tick_rmc = xTaskGetTickCount();
}

/** $xxGGA,UTC,LAT,NS,LON,EW,QUAL,NUMSV,HDOP,ALT,M,GEOID,M,AGE,REFID*cs */
static void gnss_handle_gga(char* line)
{
    char* fields[16];
    int   n = nmea_split(line, fields, 16);

    if (n < 10)
    {
        return;
    }

    gnss_data.fix_quality = (gnss_fix_t)nmea_atou(fields[6]);
    gnss_data.sats_used   = (uint8_t)nmea_atou(fields[7]);
    gnss_data.hdop        = nmea_atof(fields[8]);
    gnss_data.altitude_m  = nmea_atof(fields[9]);

    /* 有效解算时更新坐标（目标坐标解算以 GGA 为准） */
    if (gnss_data.fix_quality != GNSS_FIX_INVALID && fields[2][0] != '\0' && fields[4][0] != '\0')
    {
        double lat = nmea_coord_to_deg(fields[2]);
        double lon = nmea_coord_to_deg(fields[4]);

        if (fields[3][0] == 'S')
        {
            lat = -lat;
        }
        if (fields[5][0] == 'W')
        {
            lon = -lon;
        }
        gnss_data.latitude  = lat;
        gnss_data.longitude = lon;
    }

    gnss_data.tick_gga = xTaskGetTickCount();
}

static void gnss_handle_line(char* line, int len)
{
    if (!nmea_checksum_ok(line, len))
    {
        return;
    }

    /* 发话者 ID 任意（GN/GP/GB/BD/GL/GA），只匹配消息 ID */
    if (len >= 6 && line[3] == 'R' && line[4] == 'M' && line[5] == 'C')
    {
        gnss_handle_rmc(line);
    }
    else if (len >= 6 && line[3] == 'G' && line[4] == 'G' && line[5] == 'A')
    {
        gnss_handle_gga(line);
    }
    else
    {
        /* VTG/GSA/GSV/GLL/$POCNR 等忽略 */
    }
}

/* ------------------------------ 对外接口 ------------------------------ */

void gnss_init(void)
{
    board_gnss_power(true);
    board_uart_init(GNSS_UART, GNSS_BAUD);

    vTaskDelay(pdMS_TO_TICKS(100U));
    board_uart_flush_rx(GNSS_UART);
    memset(&gnss_data, 0, sizeof(gnss_data));
}

void gnss_poll(void)
{
    static char    line[NMEA_LINE_MAX];
    static uint8_t index = 0U;
    uint8_t        byte;

    while (board_uart_read(GNSS_UART, &byte, 1U) == 1U)
    {
        if (index == 0U && byte != '$')
        {
            continue; /* 等待行首 */
        }

        if (byte == '\r')
        {
            continue;
        }

        if (byte == '\n')
        {
            if (index > 0U)
            {
                line[index] = '\0';
                gnss_handle_line(line, index);
                index = 0U;
            }
            continue;
        }

        if (index < (NMEA_LINE_MAX - 1U))
        {
            line[index++] = (char)byte;
        }
        else
        {
            index = 0U; /* 超长丢弃 */
        }
    }
}

const gnss_data_t* gnss_get_data(void)
{
    return &gnss_data;
}

bool gnss_is_fixed(uint32_t timeout_ms)
{
    if (!gnss_data.valid || gnss_data.tick_rmc == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - gnss_data.tick_rmc) < pdMS_TO_TICKS(timeout_ms);
}
