#include "uart.h"
#include "printk.h"
#include "../include/csr.h"

void kernel_main(void) {
    uart_init();

    printk("MiniOS booting...\n");
    printk("Hello from kernel!\n");

    printk("--- Kernel Log Demo ---\n");
    printk("String: %s\n", "hello world");
    printk("Decimal: %d\n", -42);
    printk("Hex: %x\n", 0xDEADBEEF);
    printk("Long hex: %lx\n", 0x80000000UL);
    printk("Char: %c\n", 'Z');
    printk("Percent: 100%%\n");

    printk("--- Test: illegal format specifier ---\n");
    printk("Illegal %%z: %z\n", 42);
    printk("After illegal format\n");

    printk("--- Phase 3: Trap Test ---\n");
    printk("Triggering ECALL to test trap handler...\n");
    __asm__ volatile("ecall");
    printk("Returned from trap handler!\n");
    printk("Trap round-trip successful!\n");

    while (1) { }
}
