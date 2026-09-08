/**
 * @file rtt_log.c
 * @brief 最小 SEGGER RTT 上行通道实现（仅上行，无阻塞模式：缓冲满丢弃）
 */
#include "rtt_log.h"

#include "n32g4fr.h"

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
    (void)vsnprintf(buf, sizeof(buf), fmt, args); /* nano 库不支持 %f，调用方自行转整数 */
    va_end(args);
    buf[sizeof(buf) - 1U] = '\0';

    rtt_write(buf);
}
