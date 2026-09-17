/**
 * @file rtt_log.h
 * @brief 最小 SEGGER RTT 上行通道（通道 0）+ 日志输出
 *
 * 控制块与 J-Link RTT Viewer / J-Link RTT Client 兼容。
 * 日志统一走 rtt_printf。
 *
 * 说明（浮点支持）：
 * 1. 工程默认使用 newlib-nano（--specs=nano.specs），若要在 rtt_printf 中使用 %f，
 *    需在 CMake 链接选项中添加 `-Wl,-u,_printf_float`（或 `-u _printf_float`）。
 * 2. 未开启 _printf_float 时，请先将浮点数转换为整数（如乘以 1000）分步打印。
 */
#ifndef RTT_LOG_H
#define RTT_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

void rtt_log_init(void);
void rtt_write(const char* str);
void rtt_printf(const char* fmt, ...);

#define LOGI(...) rtt_printf(__VA_ARGS__)
#define LOGW(...) rtt_printf(__VA_ARGS__)
#define LOGE(...) rtt_printf(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* RTT_LOG_H */
