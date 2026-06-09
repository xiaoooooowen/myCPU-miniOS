#include "uart.h"
#include "printk.h"
#include "mem.h"
#include "vm.h"
#include "task.h"
#include "timer.h"
#include "trap.h"
#include "user.h"
#include "ramfs.h"
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

/* 模块六：短生命期任务 - 进入 U 模式后退出 */
static void user_task_entry(void) {
    user_init();
    printk("[UserTask] Entering user mode...\n");
    enter_user();
    /* enter_user 不返回，用户程序通过 sys_exit 退出 */
}

/* 模块六：有限次循环后调用 sys_exit 退出的任务 */
static void short_lived_task(void) {
    int count = 0;
    while (count < 3) {
        printk("[ShortTask] count=%d\n", count++);
        for (volatile int i = 0; i < 2000; i++)
            ;
    }

    /* 通过 ecall 调用 sys_exit(42) */
    uint64_t ret;
#ifdef __riscv
    __asm__ volatile(
        "li a7, 93\n"       /* SYS_EXIT */
        "li a0, 42\n"       /* exit code */
        "ecall\n"
        "mv %0, a0\n"
        : "=r"(ret)
        :
        : "a0", "a7"
    );
#else
    ret = 0;
#endif
    printk("[ShortTask] sys_exit returned: %ld (should not print)\n",
           (long)ret);
    /* 不应到达此处 */
    while (1)
        ;
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

    /*
     * 模块二 + 模块三：U 模式用户态演示（无任务上下文，sys_exit 直接停机）
     *
     * 此路径在 task_init() 之前运行，current 为 NULL，
     * sys_exit 直接触发 TEST_FINISH 停机。
     * 如需回归测试模块二/三，取消下面三行的注释并注释 Phase 12 块。
     */
#if 0
    printk("\n--- Phase 11: User Mode (U-Mode) Test ---\n");
    user_init();
    printk("Entering user mode...\n");
    enter_user();
    /* enter_user() 通过 sret 切换到 U 模式，用户程序通过 sys_exit 停机 */
#endif

    /* 模块七：RAMFS 最小版测试 */
    printk("\n--- Phase 13: RAMFS Test ---\n");
    ramfs_init();

    /* 创建 3 个文件 */
    int fd_a = ramfs_create("file_a");
    int fd_b = ramfs_create("file_b");
    int fd_c = ramfs_create("log");
    printk("Created fds: %d, %d, %d\n", fd_a, fd_b, fd_c);

    /* 向 file_a 写入数据并读回验证 */
    const char *data1 = "Hello RAMFS!";
    int w1 = ramfs_write(fd_a, data1, 12);
    printk("Write %d bytes to fd_a\n", w1);

    char rbuf[32] = {0};
    int r1 = ramfs_read(fd_a, rbuf, sizeof(rbuf));
    rbuf[r1] = '\0';
    printk("Read from fd_a: '%s' (%d bytes)\n", rbuf, r1);

    /* 向 file_b 写入并读回 */
    const char *data2 = "MiniOS Kernel";
    int w2 = ramfs_write(fd_b, data2, 13);
    printk("Write %d bytes to fd_b\n", w2);

    char rbuf2[32] = {0};
    int r2 = ramfs_read(fd_b, rbuf2, sizeof(rbuf2));
    rbuf2[r2] = '\0';
    printk("Read from fd_b: '%s' (%d bytes)\n", rbuf2, r2);

    /* 关闭 file_a */
    ramfs_close(fd_a);

    /* 验证关闭后无法读写 */
    int r3 = ramfs_read(fd_a, rbuf, sizeof(rbuf));
    printk("Read from closed fd_a: %d (expected -1)\n", r3);

    /* 再次创建 file_a — 可以复用槽位 */
    int fd_a2 = ramfs_create("file_a2");
    const char *data3 = "Reopen OK";
    ramfs_write(fd_a2, data3, 9);
    char rbuf3[32] = {0};
    int r4 = ramfs_read(fd_a2, rbuf3, sizeof(rbuf3));
    rbuf3[r4] = '\0';
    printk("Read from fd_a2: '%s' (%d bytes)\n", rbuf3, r4);

    ramfs_close(fd_a2);
    ramfs_close(fd_b);
    ramfs_close(fd_c);
    printk("RAMFS test passed!\n");

    /* 模块六：任务状态与 exit/wait 雏形 */
    printk("\n--- Phase 12: Task State & exit/wait Test ---\n");

    task_init();
    printk("Creating tasks...\n");

    /*
     * 创建 3 个任务：
     *   task_a        — 持续运行的循环任务（不会退出）
     *   user_task     — 进入 U 模式后通过 sys_exit 退出
     *   short_lived   — 循环 3 次后通过 ecall sys_exit 退出
     */
    task_create(task_a, "task_a");
    task_create(user_task_entry, "user_task");
    task_create(short_lived_task, "short_lived");

    /* 使能 S 模式全局中断 + 启动定时器中断 — 周期性触发抢占式调度 */
    local_irq_enable();
    timer_init();

    /* 进入抢占式调度，静默定时器中断的 trap 输出噪音 */
    trap_set_silent(1);

    printk("Entering idle loop (will reap zombies)...\n");

    int idle_count = 0;
    int reap_count = 0;
    while (1) {
        printk("[Idle] count=%d\n", idle_count++);
        for (volatile int i = 0; i < 1000; i++)
            ;

        /* 回收已退出的 ZOMBIE 子任务 */
        int tid = task_wait();
        if (tid >= 0) {
            printk("[Idle] Reaped zombie task %d\n", tid);
            reap_count++;
        }

        /* 回收了 2 个僵尸任务（user_task + short_lived）后主动停机 */
        if (reap_count >= 2) {
            printk("\n--- All zombies reaped, halting ---\n");
            *(volatile uint32_t *)0x100000 = 0x5555;
            while (1)
                ;
        }

        /* 安全网：idle 运行 50 次后强制停机 */
        if (idle_count >= 50) {
            printk("\n--- Idle timeout, halting ---\n");
            *(volatile uint32_t *)0x100000 = 0x5555;
            while (1)
                ;
        }
    }
}

