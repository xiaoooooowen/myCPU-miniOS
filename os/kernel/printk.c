#include "printk.h"
#include "uart.h"

typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

static void print_hex(uint64_t val) {
    const char hex[] = "0123456789abcdef";
    uart_putc('0');
    uart_putc('x');
    int leading = 1;
    for (int i = 15; i >= 0; i--) {
        char c = hex[(val >> (i * 4)) & 0xf];
        if (c != '0') leading = 0;
        if (!leading || i == 0) {
            uart_putc(c);
        }
    }
}

static void print_dec(uint32_t val) {
    static const uint32_t powers[] = {
        1000000000, 100000000, 10000000, 1000000,
        100000, 10000, 1000, 100, 10, 1
    };
    int started = 0;
    for (int i = 0; i < 10; i++) {
        uint32_t p = powers[i];
        char digit = '0';
        while (val >= p) {
            val -= p;
            digit++;
        }
        if (digit != '0' || started || p == 1) {
            uart_putc(digit);
            started = 1;
        }
    }
}

void printk(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt++);
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char* s = __builtin_va_arg(args, const char*);
            if (s) {
                uart_puts(s);
            } else {
                uart_puts("(null)");
            }
            break;
        }
        case 'd': {
            int d = __builtin_va_arg(args, int);
            if (d < 0) {
                uart_putc('-');
                d = -d;
            }
            print_dec((uint32_t)d);
            break;
        }
        case 'u': {
            unsigned int u = __builtin_va_arg(args, unsigned int);
            print_dec(u);
            break;
        }
        case 'x': {
            uint32_t x = __builtin_va_arg(args, uint32_t);
            print_hex(x);
            break;
        }
        case 'l': {
            fmt++;
            if (*fmt == 'x') {
                uint64_t lx = __builtin_va_arg(args, uint64_t);
                print_hex(lx);
            } else if (*fmt == 'd') {
                long ld = __builtin_va_arg(args, long);
                if (ld < 0) {
                    uart_putc('-');
                    ld = -ld;
                }
                print_dec((uint32_t)ld);
            }
            break;
        }
        case 'c': {
            char c = (char)__builtin_va_arg(args, int);
            uart_putc(c);
            break;
        }
        case '%': {
            uart_putc('%');
            break;
        }
        default:
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }
        fmt++;
    }

    __builtin_va_end(args);
}
