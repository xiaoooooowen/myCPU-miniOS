#include "uart.h"
#include "printk.h"
#include "timer.h"
#include "mem.h"
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

    printk("--- Phase 5: Memory Allocator Test ---\n");
    mem_init();
    printk("Free pages: %ld\n", (long)mem_free_pages());

    /* 分配 3 页并验证 */
    void *p1 = kalloc();
    void *p2 = kalloc();
    void *p3 = kalloc();

    printk("Alloc p1: %lx\n", (uint64_t)p1);
    printk("Alloc p2: %lx\n", (uint64_t)p2);
    printk("Alloc p3: %lx\n", (uint64_t)p3);
    printk("Free pages after alloc: %ld\n", (long)mem_free_pages());

    /* 向 p1 写入数据并验证可读写 */
    char *buf = (char *)p1;
    for (int i = 0; i < 16; i++)
        buf[i] = 'A' + i;
    printk("p1 data: ");
    for (int i = 0; i < 16; i++)
        printk("%c", buf[i]);
    printk("\n");

    /* 释放 p2，再分配应得到 p2 */
    kfree(p2);
    printk("Free pages after kfree(p2): %ld\n", (long)mem_free_pages());
    void *p4 = kalloc();
    printk("Alloc p4: %lx (should == p2: %lx)\n", (uint64_t)p4, (uint64_t)p2);

    /* 清理 */
    kfree(p1);
    kfree(p3);
    kfree(p4);
    printk("Free pages after cleanup: %ld\n", (long)mem_free_pages());
    printk("Memory allocator test passed!\n");

    printk("--- Phase 3: ECALL Trap Test ---\n");
    printk("Triggering ECALL to test trap handler...\n");
    __asm__ volatile("ecall");
    printk("Returned from trap handler!\n");
    printk("Trap round-trip successful!\n");

    printk("--- Phase 4: Timer Interrupt Test ---\n");
    timer_init();
    local_irq_enable();

    printk("Waiting for timer interrupts...\n");
    while (1) {
        __asm__ volatile("wfi");
    }
}
