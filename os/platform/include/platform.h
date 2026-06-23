#ifndef _PLATFORM_H
#define _PLATFORM_H

/* UART 平台抽象接口 */

void platform_uart_init(void);
void platform_uart_putc(char c);
int  platform_uart_getc_nonblock(void);

#endif
