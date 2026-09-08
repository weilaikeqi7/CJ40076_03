/**
 * @file rtt_log.h
 * @brief 最小 SEGGER RTT 上行通道（通道 0）+ 日志输出
 *
 * 控制块与 J-Link RTT Viewer / J-Link RTT Client 兼容。
 * 日志统一走 rtt_printf（不支持 %f，浮点请先放大为整数打印）。
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

#ifdef __cplusplus
}
#endif

#endif /* RTT_LOG_H */
