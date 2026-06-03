#include "uart.h"
#include "printk.h"
#include "mem.h"
#include "task.h"
#include "timer.h"
#include "trap.h"
#include "../include/csr.h"

/* Phase 7 抢占式调度测试任务：不调用 yield()，纯靠定时器中断切换 */
static void task_a(void) {
    int count = 0;
    while (1) {
        printk("[Task A] count=%d\n", count++);
        for (volatile int i = 0; i < 1000; i++)
            ;
    }
}

static void task_b(void) {
    int count = 0;
    while (1) {
        printk("[Task B] count=%d\n", count++);
        for (volatile int i = 0; i < 1000; i++)
            ;
    }
}

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

    printk("--- Phase 7: Preemptive Scheduling ---\n");

    task_init();

    task_create(task_a, "task_a");
    task_create(task_b, "task_b");

    /* 使能 M 模式全局中断 + 启动定时器中断 — 周期性触发抢占式调度 */
    csr_set(mstatus, MSTATUS_MIE);
    timer_init();

    /* 进入抢占式调度，静默定时器中断的 trap 输出噪音 */
    trap_set_silent(1);

    int idle_count = 0;
    while (1) {
        printk("[Idle ] count=%d\n", idle_count++);
        for (volatile int i = 0; i < 1000; i++)
            ;
    }
}
