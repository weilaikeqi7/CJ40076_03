/* 与生产 rtt_log.h 使用同一保护宏，防止 debug_log.h 与 app.c 分别解析到两份定义。 */
#ifndef RTT_LOG_H
#define RTT_LOG_H

/* 与生产 rtt_log.h 保持同名保护，测试桩直接提供同名函数与宏。 */

#include <stddef.h>
#include <stdint.h>

void rtt_log_init(void);
void test_log(const char* format, ...);
void rtt_write(const char* str);
void rtt_printf(const char* fmt, ...);
void rtt_raw_hex(const char* source, const uint8_t* data, size_t len);
void rtt_raw_line(const char* source, const char* line, size_t len);

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 0
#endif

/* 测试桩：所有日志统一走 test_log，Release 与 Debug 宏行为一致由被测代码决定 */
#define LOGI(...) test_log(__VA_ARGS__)
#define LOGW(...) test_log(__VA_ARGS__)
#define LOGE(...) test_log(__VA_ARGS__)

#endif
