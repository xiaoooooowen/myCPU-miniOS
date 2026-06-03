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
