#define UART_BASE 0x10000000
#define UART_THR  0
#define UART_LSR  5
#define UART_LSR_TX_EMPTY (1 << 5)

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

static volatile uint8_t* const uart = (volatile uint8_t*)UART_BASE;

static void uart_putc(char c) {
    while ((uart[UART_LSR] & UART_LSR_TX_EMPTY) == 0) { }
    uart[UART_THR] = c;
}

static void uart_puts(const char* s) {
    while (*s) {
        uart_putc(*s++);
    }
}

static void print_hex(uint32_t val) {
    const char hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        uart_putc(hex[(val >> (i * 4)) & 0xf]);
    }
}

void kernel_main(void) {
    uart_puts("MiniOS booting...\n");
    uart_puts("Hello from kernel!\n");

    while (1) { }
}
