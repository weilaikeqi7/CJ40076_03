/**
 * @file gnss.h
 * @brief BV-220 GNSS 模块驱动（USART1 PA9/PA10，115200 8N1）
 *
 * 模块输出 NMEA0183 V4.10：RMC/VTG/GGA/GSA/GSV/GLL（默认 1Hz，UTC 秒边界输出），
 * 发话者 ID 有 GN/GP/GB/BD/GL/GA 等。本驱动解析 RMC 与 GGA 两条关键语句，
 * 其余语句（GSA/GSV/VTG/GLL/$POCNR/$POCLK）直接忽略。
 *
 * 使用：gnss_init()（任务上下文，含上电等待）-> 主循环调 gnss_poll() ->
 *       gnss_get_data() 取最新定位数据。冷启动首次定位约 28s。
 */
#ifndef GNSS_H
#define GNSS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 定位质量（GGA QUAL_IND） */
typedef enum
{
    GNSS_FIX_INVALID = 0, /* 无解算 */
    GNSS_FIX_GNSS    = 1, /* 单点定位 */
    GNSS_FIX_DGNSS   = 2, /* 差分/SBAS */
    GNSS_FIX_PPP     = 3,
    GNSS_FIX_RTK_FIX = 4, /* RTK 固定解 */
    GNSS_FIX_RTK_FLT = 5, /* RTK 浮点解 */
    GNSS_FIX_DR      = 6, /* 惯性推算 */
} gnss_fix_t;

typedef struct
{
    /* RMC */
    bool    valid;       /* RMC STATUS：A=有效定位 */
    uint8_t utc_hour;    /* UTC 时间 */
    uint8_t utc_min;
    uint8_t utc_sec;
    uint8_t date_day;    /* UTC 日期 */
    uint8_t date_month;
    uint8_t date_year;   /* 2000 年起 */
    double  latitude;    /* 度，北纬为正 */
    double  longitude;   /* 度，东经为正 */
    float   speed_knots; /* 地速，节 */
    float   speed_kmh;   /* 地速，km/h */
    float   course_deg;  /* 地面航向，0~360 */
    char    pos_mode;    /* N/A/D/F/R/P/E，见手册表 2-14 */

    /* GGA */
    gnss_fix_t fix_quality; /* 解算质量 */
    uint8_t    sats_used;   /* 解算卫星数 */
    float      hdop;        /* 水平精度因子 */
    float      altitude_m;  /* 海拔（正高度），米 */

    uint32_t tick_rmc; /* 最近 RMC 更新 tick（0=从未收到） */
    uint32_t tick_gga; /* 最近 GGA 更新 tick */
} gnss_data_t;

/** 上电并初始化串口（115200 8N1） */
void gnss_init(void);

/** 喂串口数据解析 NMEA，主循环周期调用 */
void gnss_poll(void);

/** 取最新数据（只读指针） */
const gnss_data_t* gnss_get_data(void);

/** 是否收到有效定位且数据未超时 */
bool gnss_is_fixed(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* GNSS_H */
