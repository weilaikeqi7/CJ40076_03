/**
 * @file debug_log.h
 * @brief 可发布关闭的模块调试日志。
 *
 * CMake -DENABLE_DEBUG_LOG=ON 打开；默认关闭，发布构建不会产生日志调用。
 */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include "rtt_log.h"

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 0
#endif

#if ENABLE_DEBUG_LOG
#define DBG_LOGI(...) LOGI(__VA_ARGS__)
#define DBG_LOGW(...) LOGW(__VA_ARGS__)
#define DBG_RAW_HEX(source, data, len) rtt_raw_hex((source), (data), (len))
#define DBG_RAW_LINE(source, data, len) rtt_raw_line((source), (data), (len))
#else
#define DBG_LOGI(...) ((void)0)
#define DBG_LOGW(...) ((void)0)
#define DBG_RAW_HEX(source, data, len) ((void)0)
#define DBG_RAW_LINE(source, data, len) ((void)0)
#endif

#endif /* DEBUG_LOG_H */
