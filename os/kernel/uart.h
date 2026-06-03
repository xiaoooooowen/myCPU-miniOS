#ifndef _KERNEL_UART_H
#define _KERNEL_UART_H

#define UART_BASE 0x10000000
#define UART_THR  0
#define UART_LSR  5
#define UART_LSR_TX_EMPTY (1 << 5)

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char* s);

#endif
