/**
 * @file board_uart.c
 * @brief 板载三路串口驱动实现（RX 中断环形缓冲，TX 轮询阻塞）
 */
#include "board_uart.h"

#include "board.h"
#include "misc.h"

#define UART_RX_BUF_SIZE 256U

typedef struct
{
    USART_Module* usart;
    GPIO_Module*  tx_port;
    uint16_t      tx_pin;
    GPIO_Module*  rx_port;
    uint16_t      rx_pin;
    uint8_t       irqn;
    volatile uint16_t rx_head; /* ISR 写 */
    volatile uint16_t rx_tail; /* 任务读 */
    uint8_t           rx_buf[UART_RX_BUF_SIZE];
} uart_dev_t;

static uart_dev_t uart_devs[BOARD_UART_NUM] = {
    [BOARD_UART_GNSS] =
        {
            .usart   = USART1,
            .tx_port = BOARD_GNSS_TX_PORT,
            .tx_pin  = BOARD_GNSS_TX_PIN,
            .rx_port = BOARD_GNSS_RX_PORT,
            .rx_pin  = BOARD_GNSS_RX_PIN,
            .irqn    = USART1_IRQn,
        },
    [BOARD_UART_JY901B] =
        {
            .usart   = USART2,
            .tx_port = BOARD_JY901B_TX_PORT,
            .tx_pin  = BOARD_JY901B_TX_PIN,
            .rx_port = BOARD_JY901B_RX_PORT,
            .rx_pin  = BOARD_JY901B_RX_PIN,
            .irqn    = USART2_IRQn,
        },
    [BOARD_UART_RANGER] =
        {
            .usart   = UART6,
            .tx_port = BOARD_RANGER_TX_PORT,
            .tx_pin  = BOARD_RANGER_TX_PIN,
            .rx_port = BOARD_RANGER_RX_PORT,
            .rx_pin  = BOARD_RANGER_RX_PIN,
            .irqn    = UART6_IRQn,
        },
};

static void usart_rcc_enable(board_uart_t port)
{
    switch (port)
    {
    case BOARD_UART_GNSS:
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_USART1, ENABLE);
        break;
    case BOARD_UART_JY901B:
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
        RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);
        break;
    case BOARD_UART_RANGER:
    default:
        /* PB0/PB1 需 UART6 重映射 RMP3（UART6_RMP[1:0] = 11），AFIO 时钟必须开 */
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO | RCC_APB2_PERIPH_UART6, ENABLE);
        GPIO_ConfigPinRemap(GPIO_RMP3_UART6, ENABLE);
        break;
    }
}

void board_uart_init(board_uart_t port, uint32_t baud)
{
    uart_dev_t*    dev = &uart_devs[port];
    GPIO_InitType  gpio_init;
    USART_InitType usart_init;
    NVIC_InitType  nvic_init;

    usart_rcc_enable(port);

    /* TX：复用推挽；RX：上拉输入（空闲高电平） */
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = dev->tx_pin;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitPeripheral(dev->tx_port, &gpio_init);

    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin       = dev->rx_pin;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitPeripheral(dev->rx_port, &gpio_init);

    USART_StructInit(&usart_init);
    usart_init.BaudRate            = baud;
    usart_init.WordLength          = USART_WL_8B;
    usart_init.StopBits            = USART_STPB_1;
    usart_init.Parity              = USART_PE_NO;
    usart_init.Mode                = USART_MODE_RX | USART_MODE_TX;
    usart_init.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_Init(dev->usart, &usart_init);

    dev->rx_head = 0;
    dev->rx_tail = 0;

    /* 中断优先级 4：高于 configMAX_SYSCALL_INTERRUPT_PRIORITY(5)，
       ISR 内不调用任何 FreeRTOS API，安全 */
    nvic_init.NVIC_IRQChannel                   = dev->irqn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 4;
    nvic_init.NVIC_IRQChannelSubPriority        = 0;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic_init);

    USART_ConfigInt(dev->usart, USART_INT_RXDNE, ENABLE);
    USART_Enable(dev->usart, ENABLE);
}

void board_uart_putc(board_uart_t port, uint8_t byte)
{
    USART_Module* usart = uart_devs[port].usart;

    while (USART_GetFlagStatus(usart, USART_FLAG_TXDE) == RESET)
    {
    }
    USART_SendData(usart, byte);
}

void board_uart_write(board_uart_t port, const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;

    while (len-- > 0U)
    {
        board_uart_putc(port, *p++);
    }
}

size_t board_uart_read(board_uart_t port, void* buf, size_t max_len)
{
    uart_dev_t* dev = &uart_devs[port];
    uint8_t*    out = (uint8_t*)buf;
    size_t      n   = 0;

    while (n < max_len && dev->rx_tail != dev->rx_head)
    {
        out[n++]    = dev->rx_buf[dev->rx_tail];
        dev->rx_tail = (uint16_t)((dev->rx_tail + 1U) % UART_RX_BUF_SIZE);
    }
    return n;
}

size_t board_uart_available(board_uart_t port)
{
    uart_dev_t* dev = &uart_devs[port];

    return (uint16_t)(dev->rx_head - dev->rx_tail) % UART_RX_BUF_SIZE;
}

void board_uart_flush_rx(board_uart_t port)
{
    uart_devs[port].rx_tail = uart_devs[port].rx_head;
}

static void uart_rx_isr(board_uart_t port)
{
    uart_dev_t* dev    = &uart_devs[port];
    uint16_t    next   = (uint16_t)((dev->rx_head + 1U) % UART_RX_BUF_SIZE);
    uint8_t     byte   = (uint8_t)USART_ReceiveData(dev->usart);

    if (next != dev->rx_tail)
    {
        dev->rx_buf[dev->rx_head] = byte;
        dev->rx_head              = next;
    }
    /* 缓冲满时丢弃新字节 */
}

void USART1_IRQHandler(void)
{
    if (USART_GetIntStatus(USART1, USART_INT_RXDNE) != RESET)
    {
        uart_rx_isr(BOARD_UART_GNSS);
    }
}

void USART2_IRQHandler(void)
{
    if (USART_GetIntStatus(USART2, USART_INT_RXDNE) != RESET)
    {
        uart_rx_isr(BOARD_UART_JY901B);
    }
}

void UART6_IRQHandler(void)
{
    if (USART_GetIntStatus(UART6, USART_INT_RXDNE) != RESET)
    {
        uart_rx_isr(BOARD_UART_RANGER);
    }
}
