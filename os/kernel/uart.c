/**
 * uart.c — MiniOS UART 驱动（平台无关层）
 *
 * 底层硬件访问委托给 platform_uart_* 函数，
 * 提供阻塞 getc / 非阻塞 has_data 等内核接口。
 */

#include "uart.h"
#include "platform.h"

/* 单字符回退缓冲区：解决 "查看是否有数据" 与 "读取数据" 分离的语义 */
static int peek_char = -1;

void uart_init(void)
{
    platform_uart_init();
}

void uart_putc(char c)
{
    platform_uart_putc(c);
}

void uart_puts(const char* s)
{
    while (*s)
        uart_putc(*s++);
}

int uart_has_data(void)
{
    if (peek_char >= 0)
        return 1;

    int ch = platform_uart_getc_nonblock();
    if (ch >= 0) {
        peek_char = ch;
        return 1;
    }
    return 0;
}

char uart_getc(void)
{
    /* 先消费回退缓冲区 */
    if (peek_char >= 0) {
        char c = (char)peek_char;
        peek_char = -1;
        return c;
    }

    /* 轮询等待 */
    for (;;) {
        int ch = platform_uart_getc_nonblock();
        if (ch >= 0)
            return (char)ch;
    }
}
