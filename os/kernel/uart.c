#include "uart.h"

typedef unsigned char uint8_t;

static volatile uint8_t* const uart = (volatile uint8_t*)UART_BASE;

void uart_init(void) {
}

void uart_putc(char c) {
    while ((uart[UART_LSR] & UART_LSR_TX_EMPTY) == 0) { }
    uart[UART_THR] = c;
}

void uart_puts(const char* s) {
    while (*s) {
        uart_putc(*s++);
    }
}

int uart_has_data(void) {
    return (uart[UART_LSR] & UART_LSR_RX_READY) != 0;
}

char uart_getc(void) {
    /* 轮询等待数据就绪 */
    while (!uart_has_data()) { }
    return (char)uart[UART_RHR];
}
