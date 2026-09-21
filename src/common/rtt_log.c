/**
 * @file rtt_log.c
 * @brief 最小 SEGGER RTT 上行通道实现（仅上行，无阻塞模式：缓冲满丢弃）
 */
#include "rtt_log.h"

#if defined(N32G4FR)
#include "n32g4fr.h" // IWYU pragma: keep
#elif defined(N32L40X)
#include "n32l40x.h" // IWYU pragma: keep
#else
#error "未知的 N32 目标芯片，请在编译宏中定义 N32G4FR 或 N32L40X"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RTT_UP_BUF_SIZE 512U

typedef struct
{
    const char*       name;
    char*             buffer;
    unsigned long     size;
    volatile unsigned long wr_off;
    volatile unsigned long rd_off;
    unsigned long     flags;
} rtt_up_channel_t;

typedef struct
{
    char              id[16]; /* "SEGGER RTT" + 填充 */
    long              max_up_channels;
    long              max_down_channels;
    rtt_up_channel_t  up[1];
} rtt_cb_t;

static char rtt_up_buf[RTT_UP_BUF_SIZE];

/* J-Link 通过搜索 "SEGGER RTT" 标识定位控制块，须保持全局可见不被优化 */
__attribute__((used)) rtt_cb_t _SEGGER_RTT = {
    .id               = {'S', 'E', 'G', 'G', 'E', 'R', ' ', 'R', 'T', 'T', 0, 0, 0, 0, 0, 0},
    .max_up_channels   = 1,
    .max_down_channels = 0,
    .up = {
        {
            .name   = "Terminal",
            .buffer = rtt_up_buf,
            .size   = RTT_UP_BUF_SIZE,
            .wr_off = 0,
            .rd_off = 0,
            .flags  = 2, /* SEGGER_RTT_MODE_NO_BLOCK_TRIM */
        },
    },
};

void rtt_log_init(void)
{
    /* 控制块静态初始化已完成，此处占位保持接口完整 */
}

void rtt_write(const char* str)
{
    unsigned long wr;
    unsigned long rd;

    /* 关中断保护写游标；末尾无条件开中断，不能在要求保持关中断的上下文调用。 */
    __disable_irq();
    while (*str != '\0')
    {
        wr = _SEGGER_RTT.up[0].wr_off;
        rd = _SEGGER_RTT.up[0].rd_off;

        if (((wr + 1U) % RTT_UP_BUF_SIZE) == rd)
        {
            break; /* 缓冲满，丢弃剩余（NO_BLOCK_TRIM） */
        }

        rtt_up_buf[wr]           = *str++;
        _SEGGER_RTT.up[0].wr_off = (wr + 1U) % RTT_UP_BUF_SIZE;
    }
    __enable_irq();
}

void rtt_printf(const char* fmt, ...)
{
    char    buf[128];
    va_list args;

    va_start(args, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1U] = '\0';

    rtt_write(buf);
}

void rtt_raw_hex(const char* source, const uint8_t* data, size_t len)
{
    char   buf[128];
    size_t used = 0U;
    size_t i;
    int    n;

    if (source == NULL || data == NULL) return;
    n = snprintf(buf, sizeof(buf), "[RAW][%s] len=%u:", source, (unsigned int)len);
    if (n < 0) return;
    used = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1U;
    for (i = 0U; i < len && used + 4U < sizeof(buf); i++)
    {
        n = snprintf(buf + used, sizeof(buf) - used, " %02X", data[i]);
        if (n <= 0) break;
        used += (size_t)n;
    }
    if (used + 3U < sizeof(buf))
    {
        buf[used++] = '\r';
        buf[used++] = '\n';
        buf[used] = '\0';
    }
    else
    {
        buf[sizeof(buf) - 2U] = '\r';
        buf[sizeof(buf) - 1U] = '\n';
    }
    rtt_write(buf);
}

void rtt_raw_line(const char* source, const char* line, size_t len)
{
    char   buf[128];
    size_t copy_len;
    int    n;

    if (source == NULL || line == NULL) return;
    while (len > 0U && (line[len - 1U] == '\r' || line[len - 1U] == '\n')) len--;
    n = snprintf(buf, sizeof(buf), "[RAW][%s] ", source);
    if (n < 0) return;
    copy_len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1U;
    if (copy_len < sizeof(buf) - 1U)
    {
        size_t room = sizeof(buf) - copy_len - 3U;
        if (len > room) len = room;
        memcpy(buf + copy_len, line, len);
        copy_len += len;
        buf[copy_len++] = '\r';
        buf[copy_len++] = '\n';
        buf[copy_len] = '\0';
    }
    rtt_write(buf);
}
