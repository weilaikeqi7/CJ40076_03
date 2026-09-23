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

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 保留初始化入口；控制块和上行缓冲在静态初始化时就绪。 */
void rtt_log_init(void);
/** str 须为非空、以 NUL 结尾的字符串；缓冲满丢弃尾部，无阻塞等待。
 *  写入期间关中断，退出时无条件开中断；不得用于依赖原中断屏蔽状态的上下文。 */
void rtt_write(const char* str);
/** fmt 为有效 printf 格式串；每次最多输出 127 字节，超长内容截断。
 *  使用栈上缓冲，仍须遵守 rtt_write 的中断上下文限制。 */
void rtt_printf(const char* fmt, ...);

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 0
#endif

#if ENABLE_DEBUG_LOG
#define LOGI(...) rtt_printf(__VA_ARGS__)
#define LOGW(...) rtt_printf(__VA_ARGS__)
#define LOGE(...) rtt_printf(__VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* RTT_LOG_H */
