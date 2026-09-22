/**
 * @file debug_log.h
 * @brief 按模块独立开关的调试日志系统
 *
 * 总闸：ENABLE_DEBUG_LOG（Debug 构建自动开，Release 自动关，见 CMakeLists.txt）。
 * 分闸：每个模块一个 DBG_MOD_xxx 宏，默认全开；想关闭某个模块，
 *       直接在本文件把对应宏改成 0，或编译时 -DDBG_MOD_xxx=0。
 *
 * 使用：模块代码里一律用 LOG_xxx(...)，原始二进制帧用 RAW_xxx(...)。
 *       宏关闭时参数不求值，Release 下完全消除。
 */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include "rtt_log.h"

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 0
#endif

/* ------------------------- 各模块开关（1开 0关） ------------------------- */
#ifndef DBG_MOD_COMPASS
#define DBG_MOD_COMPASS 1   /* 电子罗盘：原始帧 + 解析角度 + 链路状态 */
#endif
#ifndef DBG_MOD_GNSS
#define DBG_MOD_GNSS    1   /* 北斗：原始 NMEA + 解析定位 + 状态 */
#endif
#ifndef DBG_MOD_RANGER
#define DBG_MOD_RANGER  1   /* 测距机：原始帧 + 解析距离 + 状态 */
#endif
#ifndef DBG_MOD_MEASURE
#define DBG_MOD_MEASURE 1   /* 测量状态机：状态迁移 + 轮次发布 */
#endif
#ifndef DBG_MOD_KEY
#define DBG_MOD_KEY     1   /* 按键事件 */
#endif
#ifndef DBG_MOD_POWER
#define DBG_MOD_POWER   1   /* 电源：电池电压/档位 + 外设供电策略 */
#endif
#ifndef DBG_MOD_THERM
#define DBG_MOD_THERM   1   /* 加热温控：温度/电压/占空比/状态 */
#endif
#ifndef DBG_MOD_HEATER
#define DBG_MOD_HEATER  0   /* 加热 PWM 设备层（频率高，默认关） */
#endif
#ifndef DBG_MOD_ADC
#define DBG_MOD_ADC     1   /* ADC 原始采样值 */
#endif
#ifndef DBG_MOD_CALIB
#define DBG_MOD_CALIB   1   /* 校准流程 */
#endif
#ifndef DBG_MOD_STORE
#define DBG_MOD_STORE   1   /* Flash 参数存取 */
#endif
#ifndef DBG_MOD_SYS
#define DBG_MOD_SYS     1   /* 系统启动/任务/看门狗 */
#endif

/* ------------------------- 底层宏 ------------------------- */
#if ENABLE_DEBUG_LOG
#define DBG_LOGI(...) rtt_printf(__VA_ARGS__)
#define DBG_LOGW(...) rtt_printf(__VA_ARGS__)
#define DBG_RAW_HEX(source, data, len)  rtt_raw_hex((source), (data), (len))
#define DBG_RAW_LINE(source, data, len) rtt_raw_line((source), (data), (len))
#else
#define DBG_LOGI(...) ((void)0)
#define DBG_LOGW(...) ((void)0)
#define DBG_RAW_HEX(source, data, len)  ((void)0)
#define DBG_RAW_LINE(source, data, len) ((void)0)
#endif

/* ------------------------- 模块日志宏 ------------------------- */
#define LOG_COMPASS(...)  do { if (ENABLE_DEBUG_LOG && DBG_MOD_COMPASS)  rtt_printf(__VA_ARGS__); } while (0)
#define LOG_GNSS(...)     do { if (ENABLE_DEBUG_LOG && DBG_MOD_GNSS)     rtt_printf(__VA_ARGS__); } while (0)
#define LOG_RANGER(...)   do { if (ENABLE_DEBUG_LOG && DBG_MOD_RANGER)   rtt_printf(__VA_ARGS__); } while (0)
#define LOG_MEASURE(...)  do { if (ENABLE_DEBUG_LOG && DBG_MOD_MEASURE)  rtt_printf(__VA_ARGS__); } while (0)
#define LOG_KEY(...)      do { if (ENABLE_DEBUG_LOG && DBG_MOD_KEY)      rtt_printf(__VA_ARGS__); } while (0)
#define LOG_POWER(...)    do { if (ENABLE_DEBUG_LOG && DBG_MOD_POWER)    rtt_printf(__VA_ARGS__); } while (0)
#define LOG_THERM(...)    do { if (ENABLE_DEBUG_LOG && DBG_MOD_THERM)    rtt_printf(__VA_ARGS__); } while (0)
#define LOG_HEATER(...)   do { if (ENABLE_DEBUG_LOG && DBG_MOD_HEATER)   rtt_printf(__VA_ARGS__); } while (0)
#define LOG_ADC(...)      do { if (ENABLE_DEBUG_LOG && DBG_MOD_ADC)      rtt_printf(__VA_ARGS__); } while (0)
#define LOG_CALIB(...)    do { if (ENABLE_DEBUG_LOG && DBG_MOD_CALIB)    rtt_printf(__VA_ARGS__); } while (0)
#define LOG_STORE(...)    do { if (ENABLE_DEBUG_LOG && DBG_MOD_STORE)    rtt_printf(__VA_ARGS__); } while (0)
#define LOG_SYS(...)      do { if (ENABLE_DEBUG_LOG && DBG_MOD_SYS)      rtt_printf(__VA_ARGS__); } while (0)

/* ------------------------- 模块原始数据宏 ------------------------- */
#define RAW_COMPASS(data, len)  do { if (ENABLE_DEBUG_LOG && DBG_MOD_COMPASS) rtt_raw_hex("COMPASS", (data), (len)); } while (0)
#define RAW_COMPASS_TX(data, len) do { if (ENABLE_DEBUG_LOG && DBG_MOD_COMPASS) rtt_raw_hex("COMPASS_TX", (data), (len)); } while (0)
#define RAW_GNSS(data, len)     do { if (ENABLE_DEBUG_LOG && DBG_MOD_GNSS)    rtt_raw_line("BEIDOU", (data), (len)); } while (0)
#define RAW_RANGER(data, len)   do { if (ENABLE_DEBUG_LOG && DBG_MOD_RANGER)  rtt_raw_hex("RANGER", (data), (len)); } while (0)
#define RAW_RANGER_TX(data, len) do { if (ENABLE_DEBUG_LOG && DBG_MOD_RANGER) rtt_raw_hex("RANGER_TX", (data), (len)); } while (0)

#endif /* DEBUG_LOG_H */
