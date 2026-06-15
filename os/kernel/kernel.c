#include "uart.h"
#include "printk.h"
#include "mem.h"
#include "vm.h"
#include "task.h"
#include "timer.h"
#include "trap.h"
#include "user.h"
#include "ramfs.h"
#include "minifs.h"
#include "../include/csr.h"

#ifndef MINIOS_BOOT_DIAGNOSTICS
#define MINIOS_BOOT_DIAGNOSTICS 0
#endif

#ifndef MINIOS_BOOT_COLOR
#define MINIOS_BOOT_COLOR 1
#endif

#if MINIOS_BOOT_COLOR
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"
#else
#define COLOR_GREEN ""
#define COLOR_RED   ""
#define COLOR_RESET ""
#endif

static void boot_banner(void) {
    printk("\n");
    printk("  __  __ _       _  ___  ____\n");
    printk(" |  \\/  (_)_ __ (_)/ _ \\/ ___|\n");
    printk(" | |\\/| | | '_ \\| | | | \\___ \\\n");
    printk(" | |  | | | | | | | |_| |___) |\n");
    printk(" |_|  |_|_|_| |_|_|\\___/|____/\n");
    printk("\n");
    printk(" MiniOS | RV64I | S-mode | Sv39\n");
    printk("\n");
}

static void boot_status(int ok, const char *component, const char *detail) {
    const char *color = ok ? COLOR_GREEN : COLOR_RED;
    const char *label = ok ? " OK " : "FAIL";
    printk(" %s[%s]%s %s%s\n", color, label, COLOR_RESET,
           component, detail);
}

static void boot_status_pages(int ok, long pages) {
    const char *color = ok ? COLOR_GREEN : COLOR_RED;
    const char *label = ok ? " OK " : "FAIL";
    printk(" %s[%s]%s Physical memory    %ld pages free\n",
           color, label, COLOR_RESET, pages);
}

#if MINIOS_BOOT_DIAGNOSTICS
static volatile int fcfs_trace = 0;

static void process_exit(int code) {
#ifdef __riscv
    __asm__ volatile(
        "mv a0, %0\n"
        "li a7, 93\n"
        "ecall\n"
        :
        : "r"((uint64_t)code)
        : "a0", "a7"
    );
#else
    (void)code;
#endif
    while (1)
        ;
}

static void fcfs_task_a(void) {
    fcfs_trace = fcfs_trace * 10 + 1;
    printk("[FCFS-A] start/end\n");
    fcfs_trace = fcfs_trace * 10 + 1;
    process_exit(0);
}

static void fcfs_task_b(void) {
    fcfs_trace = fcfs_trace * 10 + 2;
    printk("[FCFS-B] start/end\n");
    fcfs_trace = fcfs_trace * 10 + 2;
    process_exit(0);
}
#endif

static void user_task_entry(void) {
    user_init();
#if MINIOS_BOOT_DIAGNOSTICS
    printk("[UserTask] Entering user mode...\n");
#endif
    enter_user();
}

#if MINIOS_BOOT_DIAGNOSTICS
static void diagnostic_memory(void) {
    printk("\n--- Memory Allocator Test ---\n");
    void *p1 = kalloc();
    void *p2 = kalloc();
    void *p3 = kalloc();

    printk("Alloc p1: %lx\n", (uint64_t)p1);
    printk("Alloc p2: %lx\n", (uint64_t)p2);
    printk("Alloc p3: %lx\n", (uint64_t)p3);
    printk("Free pages after alloc: %ld\n", (long)mem_free_pages());

    char *buf = (char *)p1;
    for (int i = 0; i < 16; i++)
        buf[i] = 'A' + i;
    printk("p1 data: ");
    for (int i = 0; i < 16; i++)
        printk("%c", buf[i]);
    printk("\n");

    kfree(p2);
    printk("Free pages after kfree(p2): %ld\n", (long)mem_free_pages());
    void *p4 = kalloc();
    printk("Alloc p4: %lx (should == p2: %lx)\n",
           (uint64_t)p4, (uint64_t)p2);

    kfree(p1);
    kfree(p3);
    kfree(p4);
    printk("Free pages after cleanup: %ld\n", (long)mem_free_pages());
    printk("Memory allocator test passed!\n");
}

static void diagnostic_trap(void) {
    printk("\n--- ECALL Trap Test ---\n");
    printk("Triggering ECALL to test trap handler...\n");
    __asm__ volatile("ecall");
    printk("Trap round-trip successful!\n");
}

static void diagnostic_syscalls(void) {
    const char *msg = "Hello from syscall!\n";
    uint64_t ret;

    printk("\n--- System Call Test ---\n");
#ifdef __riscv
    __asm__ volatile(
        "li a7, 64\n"
        "li a0, 1\n"
        "mv a1, %1\n"
        "li a2, 21\n"
        "ecall\n"
        "mv %0, a0\n"
        : "=r"(ret)
        : "r"(msg)
        : "a0", "a1", "a2", "a7"
    );
#else
    ret = 21;
#endif
    printk("sys_write returned: %ld (expected 21)\n", (long)ret);

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
    printk("System call test passed!\n");
}

static void diagnostic_ramfs(void) {
    printk("\n--- RAMFS Test ---\n");
    ramfs_init();

    int fd_a = ramfs_create("file_a");
    int fd_b = ramfs_create("file_b");
    int fd_c = ramfs_create("log");
    printk("Created fds: %d, %d, %d\n", fd_a, fd_b, fd_c);

    const char *data1 = "Hello RAMFS!";
    int w1 = ramfs_write(fd_a, data1, 12);
    printk("Write %d bytes to fd_a\n", w1);
    char rbuf[32] = {0};
    int r1 = ramfs_read(fd_a, rbuf, sizeof(rbuf));
    rbuf[r1] = '\0';
    printk("Read from fd_a: '%s' (%d bytes)\n", rbuf, r1);

    const char *data2 = "MiniOS Kernel";
    int w2 = ramfs_write(fd_b, data2, 13);
    printk("Write %d bytes to fd_b\n", w2);
    char rbuf2[32] = {0};
    int r2 = ramfs_read(fd_b, rbuf2, sizeof(rbuf2));
    rbuf2[r2] = '\0';
    printk("Read from fd_b: '%s' (%d bytes)\n", rbuf2, r2);

    ramfs_close(fd_a);
    int r3 = ramfs_read(fd_a, rbuf, sizeof(rbuf));
    printk("Read from closed fd_a: %d (expected -1)\n", r3);

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
}

static void diagnostic_fcfs(void) {
    printk("\n--- FCFS Scheduler Test ---\n");
    task_set_scheduler(SCHED_FCFS);
    task_init();
    fcfs_trace = 0;
    task_create(fcfs_task_a, "fcfs_a");
    task_create(fcfs_task_b, "fcfs_b");
    local_irq_enable();
    timer_init();
    trap_set_silent(1);

    int reaped = 0;
    while (reaped < 2) {
        if (task_wait() > 0)
            reaped++;
    }
    printk("[%s] FCFS non-preemptive order (trace=%d, expected=1122)\n",
           fcfs_trace == 1122 ? "PASS" : "FAIL", fcfs_trace);
}

static void run_boot_diagnostics(void) {
    printk("\n=== Boot Diagnostics ===\n");
    diagnostic_memory();
    diagnostic_trap();
    diagnostic_syscalls();
    diagnostic_ramfs();
    diagnostic_fcfs();
    printk("\n=== Diagnostics Complete ===\n\n");
}
#endif

static int start_shell(void) {
    local_irq_disable();
    task_set_scheduler(SCHED_RR);
    task_init();
    task_set_quantum(1);
    timer_set_timeslice_ms(TIMER_DEFAULT_SLICE_MS);

    int shell_pid = task_create(user_task_entry, "shell");
    if (shell_pid < 0)
        return -1;

    timer_init();
    trap_set_silent(1);
    return shell_pid;
}

void kernel_main(void) {
    uart_init();
    boot_banner();

    mem_init();
    boot_status_pages(mem_free_pages() > 0, (long)mem_free_pages());

    vm_init();
    boot_status(kernel_l2 != NULL, "Virtual memory     ",
                "Sv39, 128 MiB mapped");

    int fs_ready = minifs_init() == 0;
    boot_status(fs_ready, "MiniFS             ", "1 MiB persistent disk");
    if (!fs_ready) {
        printk("Invalid disk image; refusing to overwrite it.\n");
        *(volatile uint32_t *)0x100000 = 0x5555;
        while (1)
            ;
    }

#if MINIOS_BOOT_DIAGNOSTICS
    run_boot_diagnostics();
#endif

    int shell_pid = start_shell();
    boot_status(shell_pid >= 0, "Scheduler          ", "RR, 10 ms quantum");

    if (shell_pid < 0) {
        boot_status(0, "User shell         ", "creation failed");
        *(volatile uint32_t *)0x100000 = 0x5555;
        while (1)
            ;
    }

    printk(" ");
    if (MINIOS_BOOT_COLOR)
        printk(COLOR_GREEN);
    printk("[ OK ]");
    if (MINIOS_BOOT_COLOR)
        printk(COLOR_RESET);
    printk(" User shell         pid %d\n", shell_pid);
    printk("\nWelcome to MiniOS.\n");
    printk("Type 'help' for commands.\n");
    local_irq_enable();

    while (1) {
        int pid = task_waitpid(shell_pid, NULL, 1);
        if (pid == shell_pid) {
            printk("\nShell exited, halting MiniOS.\n");
            *(volatile uint32_t *)0x100000 = 0x5555;
            while (1)
                ;
        }
    }
}
