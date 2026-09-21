/**
 * @file dev_gnss.c
 * @brief BV-220 GNSS 模块设备驱动实现（NMEA0183 V4.10 行解析 + 异或校验）
 */
#include "dev_gnss.h"

#include "bsp_power.h"
#include "bsp_uart.h"
#include "debug_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

#define GNSS_UART BSP_UART_GNSS
#define GNSS_BAUD 115200U

#define NMEA_LINE_MAX 96U

static gnss_data_t gnss_data;
static bool         gnss_powered;
static uint32_t     gnss_generation;
static TickType_t   gnss_ready_tick;
static bool         gnss_settling;

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
    /* 每行控制在 RTT 的 127 字节限制内；未定位时不把空字段转换值当作有效读数。 */
    if (!gnss_data.valid || fields[3][0] == '\0' || fields[5][0] == '\0')
    {
        DBG_LOGI("[DATA][BEIDOU][RMC] valid=0 position_unavailable\r\n");
    }
    else
    {
        DBG_LOGI("[DATA][BEIDOU][RMC] valid=1 lat=%.7f lon=%.7f\r\n",
                 gnss_data.latitude, gnss_data.longitude);
        DBG_LOGI("[DATA][BEIDOU][RMC] utc=%02u:%02u:%02u date=20%02u-%02u-%02u\r\n",
                 (unsigned int)gnss_data.utc_hour, (unsigned int)gnss_data.utc_min,
                 (unsigned int)gnss_data.utc_sec, (unsigned int)gnss_data.date_year,
                 (unsigned int)gnss_data.date_month, (unsigned int)gnss_data.date_day);
        DBG_LOGI("[DATA][BEIDOU][RMC] speed_kn=%.3f speed_kmh=%.3f course=%.3f mode=%c\r\n",
                 gnss_data.speed_knots, gnss_data.speed_kmh, gnss_data.course_deg,
                 gnss_data.pos_mode != '\0' ? gnss_data.pos_mode : '-');
    }
}

/** $xxGGA,UTC,LAT,NS,LON,EW,QUAL,NUMSV,HDOP,ALT,M,GEOID,M,AGE,REFID*cs */
static void gnss_handle_gga(char* line, uint32_t generation)
{
    char*      fields[16];
    int        n = nmea_split(line, fields, 16);
    gnss_fix_t fix_quality;
    uint8_t    sats_used;
    float      hdop;
    float      altitude_m;
    double     latitude = 0.0;
    double     longitude = 0.0;
    bool       has_position;
    uint32_t   tick_gga;

    if (n < 10)
    {
        return;
    }

    /* 在临界区外完成解析，仅在下面的发布阶段加锁。 */
    fix_quality = (gnss_fix_t)nmea_atou(fields[6]);
    sats_used   = (uint8_t)nmea_atou(fields[7]);
    hdop        = nmea_atof(fields[8]);
    altitude_m  = nmea_atof(fields[9]);
    has_position = fix_quality != GNSS_FIX_INVALID && fields[2][0] != '\0' && fields[4][0] != '\0';
    if (has_position)
    {
        latitude  = nmea_coord_to_deg(fields[2]);
        longitude = nmea_coord_to_deg(fields[4]);

        if (fields[3][0] == 'S')
        {
            latitude = -latitude;
        }
        if (fields[5][0] == 'W')
        {
            longitude = -longitude;
        }
    }
    tick_gga = xTaskGetTickCount();

    /* 只在同一供电代次内提交，避免下电并发时发布过期语句。 */
    taskENTER_CRITICAL();
    if (!gnss_powered || generation != gnss_generation)
    {
        taskEXIT_CRITICAL();
        return;
    }

    /* 缺少坐标时不能沿用旧坐标冒充新定位。 */
    gnss_data.fix_quality = has_position ? fix_quality : GNSS_FIX_INVALID;
    gnss_data.sats_used   = sats_used;
    gnss_data.hdop        = hdop;
    gnss_data.altitude_m  = altitude_m;
    if (has_position)
    {
        gnss_data.latitude  = latitude;
        gnss_data.longitude = longitude;
    }
    gnss_data.tick_gga = tick_gga;
    taskEXIT_CRITICAL();

    if (!has_position)
    {
        DBG_LOGI("[DATA][BEIDOU][GGA] valid=0 fix=%u sats=%u hdop=%.2f position_unavailable\r\n",
                 (unsigned int)fix_quality, (unsigned int)sats_used, hdop);
    }
    else
    {
        DBG_LOGI("[DATA][BEIDOU][GGA] valid=1 fix=%u sats=%u hdop=%.2f\r\n",
                 (unsigned int)fix_quality, (unsigned int)sats_used, hdop);
        DBG_LOGI("[DATA][BEIDOU][GGA] lat=%.7f lon=%.7f alt=%.3fm\r\n",
                 latitude, longitude, altitude_m);
    }
}

static void gnss_handle_line(char* line, int len, uint32_t generation)
{
    bool powered;

    /* 原始行先打印，再做校验；校验失败的原始数据也能用于定位链路问题。 */
    DBG_RAW_LINE("BEIDOU", line, (size_t)len);

    if (!nmea_checksum_ok(line, len))
    {
        DBG_LOGW("[DATA][BEIDOU] checksum_invalid\r\n");
        return;
    }

    taskENTER_CRITICAL();
    powered = gnss_powered && generation == gnss_generation;
    taskEXIT_CRITICAL();
    if (!powered)
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
        gnss_handle_gga(line, generation);
    }
    else
    {
        /* VTG/GSA/GSV/GLL/$POCNR 等忽略 */
    }
}

/* ------------------------------ 对外接口 ------------------------------ */

/**
 * @brief 发送 NMEA 配置字符串（自动追加 \\r\\n）
 */
static void gnss_send_cmd(const char* cmd)
{
    DBG_LOGI("[DBG][BEIDOU] TX %s", cmd);
    bsp_uart_write(GNSS_UART, cmd, strlen(cmd));
}

void gnss_init(void)
{
    bsp_pwr_gnss(true);
    bsp_uart_init(GNSS_UART, GNSS_BAUD);

    vTaskDelay(pdMS_TO_TICKS(200U));

    /*
     * 根据 B 系列配置手册：只保留 GGA 语句，关闭其余冗余语句：
     * 防范环形缓冲区被 GSV/GSA 等多行冗余数据打爆
     */
    gnss_send_cmd("$POLCFGMSG,0,0,1\r\n"); /* 确保 GGA 开启 */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGMSG,0,1,0\r\n"); /* 关闭 GSA */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGMSG,0,2,0\r\n"); /* 关闭 GSV (最大冗余源) */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGMSG,0,3,0\r\n"); /* 关闭 VTG */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGMSG,0,5,0\r\n"); /* 关闭 RMC */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGMSG,0,13,0\r\n"); /* 关闭 GLL */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGNAV,10\r\n"); /* 10HZ输出 */
    vTaskDelay(pdMS_TO_TICKS(200U));
    gnss_send_cmd("$POLCFGSAVE\r\n"); /* 保存配置 */
    vTaskDelay(pdMS_TO_TICKS(200U));

    taskENTER_CRITICAL();
    bsp_uart_flush_rx(GNSS_UART);
    memset(&gnss_data, 0, sizeof(gnss_data));
    gnss_powered    = true;
    gnss_generation++;
    gnss_ready_tick = xTaskGetTickCount();
    gnss_settling = false;
    taskEXIT_CRITICAL();
}

void gnss_power_ctl(bool on)
{
    taskENTER_CRITICAL();
    bsp_pwr_gnss(on);
    gnss_powered = on;
    gnss_settling = on;
    gnss_generation++;
    if (on)
    {
        gnss_ready_tick = xTaskGetTickCount() + pdMS_TO_TICKS(100U);
    }
    else
    {
        gnss_ready_tick = 0U;
        memset(&gnss_data, 0, sizeof(gnss_data));
        bsp_uart_flush_rx(GNSS_UART);
    }
    taskEXIT_CRITICAL();
}

#if ENABLE_DEBUG_LOG
static void gnss_debug_status(bool powered, bool settling)
{
    static TickType_t last_tick;
    static bool first = true;
    TickType_t now = xTaskGetTickCount();
    gnss_data_t data;

    if (!first && (TickType_t)(now - last_tick) < pdMS_TO_TICKS(1000U)) return;
    first = false;
    last_tick = now;
    taskENTER_CRITICAL();
    data = gnss_data;
    taskEXIT_CRITICAL();
    DBG_LOGI("[STATUS][BEIDOU] power=%u settling=%u rx=%u fix=%u sats=%u age_gga_ms=%lu lat=%.7f lon=%.7f alt=%.2fm\r\n",
             powered ? 1U : 0U, settling ? 1U : 0U,
             (unsigned int)bsp_uart_available(GNSS_UART), (unsigned int)data.fix_quality,
             (unsigned int)data.sats_used,
             data.tick_gga == 0U ? 0UL : (unsigned long)(((uint64_t)(now - data.tick_gga) * 1000U) /
                                                         configTICK_RATE_HZ),
             data.latitude, data.longitude, data.altitude_m);
}
#endif

void gnss_poll(void)
{
    static char     line[NMEA_LINE_MAX];
    static uint8_t  index = 0U;
    static uint32_t parser_generation;
    bool           powered;
    bool           settling;
    uint32_t       generation;
    uint8_t        byte;

    taskENTER_CRITICAL();
    powered    = gnss_powered;
    generation = gnss_generation;
    if (gnss_settling && (int32_t)(xTaskGetTickCount() - gnss_ready_tick) >= 0)
    {
        gnss_settling = false;
    }
    settling = gnss_settling;
    taskEXIT_CRITICAL();
    /* 两次轮询之间也可能上下电，必须丢弃上一供电代次留下的半行。 */
    if (parser_generation != generation)
    {
        index = 0U;
        parser_generation = generation;
    }
#if ENABLE_DEBUG_LOG
    gnss_debug_status(powered, settling);
#endif
    if (!powered || settling)
    {
        return;
    }

    for (;;)
    {
        bool     have_byte;
        bool     current_powered;
        uint32_t current_generation;

        /* 每次读一个字节，避免与下电清空接收缓存发生竞态。 */
        taskENTER_CRITICAL();
        have_byte = bsp_uart_read(GNSS_UART, &byte, 1U) == 1U;
        current_powered = gnss_powered;
        current_generation = gnss_generation;
        taskEXIT_CRITICAL();
        if (!current_powered || current_generation != generation)
        {
            /* 供电代次变化后丢弃半行，防止重新上电时拼接旧语句。 */
            index = 0U;
            return;
        }
        if (!have_byte)
        {
            break;
        }
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
                gnss_handle_line(line, index, generation);
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

void gnss_get_fix_snapshot(gnss_fix_snapshot_t* out)
{
    if (out == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    out->latitude    = gnss_data.latitude;
    out->longitude   = gnss_data.longitude;
    out->altitude_m  = gnss_data.altitude_m;
    out->fix_quality = gnss_data.fix_quality;
    out->tick_gga    = gnss_data.tick_gga;
    taskEXIT_CRITICAL();
}

bool gnss_is_fixed(uint32_t timeout_ms)
{
    bool     valid;
    uint32_t tick_rmc;

    taskENTER_CRITICAL();
    valid    = gnss_data.valid;
    tick_rmc = gnss_data.tick_rmc;
    taskEXIT_CRITICAL();

    if (!valid || tick_rmc == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - tick_rmc) < pdMS_TO_TICKS(timeout_ms);
}
