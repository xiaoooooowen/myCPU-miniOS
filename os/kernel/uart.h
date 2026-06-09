#ifndef _KERNEL_UART_H
#define _KERNEL_UART_H

#define UART_BASE 0x10000000
#define UART_THR  0
#define UART_RHR  0          /* 接收保持寄存器（与 THR 同地址，读写区分） */
#define UART_LSR  5
#define UART_LSR_TX_EMPTY (1 << 5)
#define UART_LSR_RX_READY (1) /* RX 数据就绪位 */

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char* s);

/*
 * 输入相关函数：
 *   uart_has_data() — 检查是否有待读取的输入字符
 *   uart_getc()     — 读取一个字符（阻塞直到有数据）
 */
int uart_has_data(void);
char uart_getc(void);

#endif
