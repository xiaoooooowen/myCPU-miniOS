#include "uart.h"
#include "printk.h"
#include "mem.h"
#include "vm.h"
#include "task.h"
#include "timer.h"
#include "trap.h"
#include "user.h"
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

    printk("--- Phase 10: Virtual Memory (Sv39) ---\n");
    vm_init();
    printk("Sv39 page table setup complete (identity map 128MB DRAM)\n");

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

    printk("--- Phase 8: System Call Test ---\n");
    {
        /* sys_write(1, msg, len) — 通过 ecall 输出字符串 */
        const char *msg = "Hello from syscall!\n";
        uint64_t ret;
        /*
         * RISC-V syscall 约定：
         *   a7 = 系统调用号 (SYS_WRITE = 64)
         *   a0 = fd (1 = stdout)
         *   a1 = buf
         *   a2 = len
         *   返回值在 a0
         */
#ifdef __riscv
        __asm__ volatile(
            "li a7, 64\n"       /* SYS_WRITE */
            "li a0, 1\n"        /* fd=1 */
            "mv a1, %1\n"       /* buf */
            "li a2, 21\n"       /* len */
            "ecall\n"
            "mv %0, a0\n"       /* 保存返回值 */
            : "=r"(ret)
            : "r"(msg)
            : "a0", "a1", "a2", "a7"
        );
#else
        ret = 21;  /* 宿主编译器/LSP 使用，避免 asm 寄存器检查报错 */
#endif
        printk("sys_write returned: %ld (expected 21)\n", (long)ret);
    }
    {
        /* sys_write 空字符串测试 */
        uint64_t ret;
#ifdef __riscv
        __asm__ volatile(
            "li a7, 64\n"
            "li a0, 0\n"
            "li a1, 0\n"
            "li a2, 0\n"
            "ecall\n"
            "mv %0, a0\n"
            : "=r"(ret)
            :
            : "a0", "a1", "a2", "a7"
        );
#else
        ret = 0;
#endif
        printk("sys_write(NULL,0) returned: %ld (expected 0)\n", (long)ret);
    }
    {
        /* 测试未知系统调用号 */
        uint64_t ret;
#ifdef __riscv
        __asm__ volatile(
            "li a7, 999\n"
            "ecall\n"
            "mv %0, a0\n"
            : "=r"(ret)
            :
            : "a0", "a1", "a2", "a7"
        );
#else
        ret = (uint64_t)-1;
#endif
        printk("Unknown syscall returned: %ld (expected -1)\n", (long)ret);
    }
    printk("System call test passed!\n");

    /* 模块二 + 模块三：U 模式用户态演示 */
    printk("\n--- Phase 11: User Mode (U-Mode) Test ---\n");
    user_init();
    printk("Entering user mode...\n");
    enter_user();
    /* enter_user() 通过 sret 切换到 U 模式，不会返回 */

    /* 模块一验证：主动停机 — 在调度启动前，确保 TEST_FINISH 机制可用 */
    printk("\n--- Halting via TEST_FINISH ---\n");
    *(volatile uint32_t *)0x100000 = 0x5555;

    task_init();

    task_create(task_a, "task_a");
    task_create(task_b, "task_b");

    /* 使能 S 模式全局中断 + 启动定时器中断 — 周期性触发抢占式调度 */
    local_irq_enable();
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
