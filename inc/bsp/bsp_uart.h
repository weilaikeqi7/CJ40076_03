/**
 * @file bsp_uart.h
 * @brief N32G4FR MCU 片上串口驱动（USART1/2/UART6）
 *
 * 职责边界：
 *   - 仅封装 MCU 片上硬件串口初始化、收发寄存器操作与环形缓冲管理；
 *   - 不包含 GNSS、罗盘或测距机的任何通讯协议帧或指令时序。
 * 所有接口的 port 必须小于 BSP_UART_NUM；收发前须初始化。
 * 接收缓冲仅支持单任务消费；发送不加锁，同端口多任务发送须自行串行化。
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BSP_UART_GNSS = 0, /* USART1 PA9/PA10 */
    BSP_UART_COMPASS,  /* USART2 PA2/PA3（电子罗盘） */
    BSP_UART_RANGER,   /* UART6  PB0/PB1（RMP3） */
    BSP_UART_NUM
} bsp_uart_t;

/**
 * @brief 初始化串口并使能接收中断（RXDNE -> 环形缓冲）。
 * @param port 串口编号
 * @param baud 波特率（8N1，无流控）
 */
void bsp_uart_init(bsp_uart_t port, uint32_t baud);

/** 轮询发送 len 字节；len>0 时 data 必须有效，无超时或任务让出。
 *  返回时最后一字节可能尚在移位输出；断电前需上层保证发送完成。 */
void bsp_uart_write(bsp_uart_t port, const void* data, size_t len);

/** 发送一个字节（阻塞） */
void bsp_uart_putc(bsp_uart_t port, uint8_t byte);

/**
 * @brief 从接收环形缓冲读出数据。
 * @param buf 输出缓冲；max_len>0 时须有效且至少容纳 max_len 字节。
 * @param max_len 最多读取字节数；立即返回当前可用数据，不等待凑满。
 * @return 实际读出字节数（0 表示暂无数据或 max_len 为 0）
 */
size_t bsp_uart_read(bsp_uart_t port, void* buf, size_t max_len);

/** 接收缓冲中暂存字节数 */
size_t bsp_uart_available(bsp_uart_t port);

/** 清空接收缓冲 */
void bsp_uart_flush_rx(bsp_uart_t port);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UART_H */
