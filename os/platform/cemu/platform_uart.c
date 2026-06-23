/**
 * platform_uart.c — cemu 平台 UART 底层实现
 *
 * 直接访问 cemu 模拟器的 UART MMIO 寄存器。
 */

#include "platform.h"

/* UART 寄存器定义 — cemu MMIO 基址 0x10000000 */
#define UART_BASE       0x10000000UL
#define UART_THR        0               /* 发送保持寄存器 (写) */
#define UART_RHR        0               /* 接收保持寄存器 (读) */
#define UART_LSR        5               /* 线状态寄存器 */
#define UART_LSR_TX_EMPTY (1 << 5)
#define UART_LSR_RX_READY (1)

typedef unsigned char uint8_t;

static volatile uint8_t* const uart = (volatile uint8_t*)UART_BASE;

void platform_uart_init(void)
{
    /* cemu UART 无需额外初始化 */
}

void platform_uart_putc(char c)
{
    while ((uart[UART_LSR] & UART_LSR_TX_EMPTY) == 0) { }
    uart[UART_THR] = (uint8_t)c;
}

int platform_uart_getc_nonblock(void)
{
    if (uart[UART_LSR] & UART_LSR_RX_READY)
        return (int)(unsigned char)uart[UART_RHR];
    return -1;  /* 无数据 */
}
