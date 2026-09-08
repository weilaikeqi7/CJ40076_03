/**
 * @file board_uart.h
 * @brief 板载三路串口驱动：GNSS(USART1) / JY901B(USART2) / 测距机(UART6 重映射 PB0/PB1)
 *
 * 接收采用中断 + 环形缓冲（256 字节），任务侧调用 board_uart_read 取数据；
 * 发送为阻塞式（轮询 TXDE）。
 */
#ifndef BOARD_UART_H
#define BOARD_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BOARD_UART_GNSS = 0, /* USART1 PA9/PA10，默认 9600 */
    BOARD_UART_JY901B,   /* USART2 PA2/PA3，默认 9600 */
    BOARD_UART_RANGER,   /* UART6  PB0/PB1（RMP3），默认 115200 */
    BOARD_UART_NUM
} board_uart_t;

/**
 * @brief 初始化串口并使能接收中断（RXDNE -> 环形缓冲）。
 * @param port 串口编号
 * @param baud 波特率（8N1，无流控）
 */
void board_uart_init(board_uart_t port, uint32_t baud);

/** 阻塞发送 len 字节 */
void board_uart_write(board_uart_t port, const void* data, size_t len);

/** 发送一个字节（阻塞） */
void board_uart_putc(board_uart_t port, uint8_t byte);

/**
 * @brief 从接收环形缓冲读出数据。
 * @return 实际读出字节数（0 表示暂无数据）
 */
size_t board_uart_read(board_uart_t port, void* buf, size_t max_len);

/** 接收缓冲中暂存字节数 */
size_t board_uart_available(board_uart_t port);

/** 清空接收缓冲 */
void board_uart_flush_rx(board_uart_t port);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_UART_H */
