#include "syscall.h"
#include "uart.h"
#include "printk.h"
#include "task.h"
#include "ramfs.h"

/* TEST_FINISH 设备地址：向此地址写入任意值通知模拟器停止运行 */
#define TEST_FINISH 0x100000

/*
 * sys_write() — 向控制台 (fd=1) 或 RAMFS 文件 (fd>=2) 输出
 *
 * 参数：
 *   fd   文件描述符（1=stdout，>=2=RAMFS 文件）
 *   buf  指向字符串缓冲区
 *   len  要输出的字节数
 *
 * 返回值：实际输出的字节数
 */
static uint64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    if (buf == 0 || len == 0)
        return 0;

    /* fd=1: 控制台输出 */
    if (fd == 1) {
        const char *s = (const char *)buf;
        uint64_t written = 0;
        for (uint64_t i = 0; i < len; i++) {
            uart_putc(s[i]);
            written++;
        }
        return written;
    }

    /* fd>=2: RAMFS 文件写入 */
    return (uint64_t)ramfs_write((int)fd, (const void *)buf, len);
}

/*
 * sys_read() — 从控制台 (fd=0) 或 RAMFS 文件 (fd>=2) 读取
 *
 * 参数：
 *   fd   文件描述符（0=stdin，>=2=RAMFS 文件）
 *   buf  字符缓冲区
 *   len  最大读取字节数
 *
 * 返回值：实际读取的字节数，失败返回 -1
 */
static uint64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len) {
    if (buf == 0 || len == 0)
        return 0;

    /* fd=0: 控制台输入 */
    if (fd == 0) {
        char *dst = (char *)buf;
        for (uint64_t i = 0; i < len; i++) {
            dst[i] = uart_getc();  /* 阻塞等待输入 */
            if (dst[i] == '\n')
                return i + 1;
        }
        return len;
    }

    /* fd>=2: RAMFS 文件读取 */
    return (uint64_t)ramfs_read((int)fd, (void *)buf, len);
}

/*
 * sys_exit() — 退出当前任务
 *
 * 参数：
 *   code  退出码
 *
 * 若当前有运行中的任务（current != NULL），标记为 ZOMBIE，
 * 由 trap_handler 在返回前触发重调度。
 * 否则（无任务上下文），直接触发 TEST_FINISH 停机。
 */
static void sys_exit(uint64_t code) {
    printk("\n--- Task Exit (code=%ld) ---\n", (long)code);

    /* 尝试将当前任务标记为 ZOMBIE */
    task_exit();

    /* 若 task_exit 未成功标记（current 为 NULL），则直接停机 */
    if (task_current_state() != TASK_ZOMBIE) {
        *(volatile uint32_t *)TEST_FINISH = 0x5555;
        while (1)
            ;
    }
}

/*
 * sys_wait() — 回收一个已退出的 ZOMBIE 子任务
 *
 * 参数：忽略
 *
 * 返回值：
 *   被回收的任务 tid，无 ZOMBIE 子任务返回 -1
 */
static uint64_t sys_wait(uint64_t ignored) {
    (void)ignored;
    return (uint64_t)task_wait();
}

/*
 * sys_open() — 创建 RAMFS 文件
 *
 * 参数：
 *   name  文件名指针（用户态字符串）
 *
 * 返回值：文件描述符 fd（>=2），失败返回 -1
 */
static uint64_t sys_open(uint64_t name, uint64_t flags) {
    (void)flags;
    if (name == 0)
        return (uint64_t)-1;
    return (uint64_t)ramfs_create((const char *)name);
}

/*
 * sys_close() — 关闭 RAMFS 文件
 *
 * 参数：
 *   fd   文件描述符
 *
 * 返回值：成功返回 0，失败返回 -1
 */
static uint64_t sys_close(uint64_t fd) {
    if ((int)fd < 2)
        return (uint64_t)-1;
    return (uint64_t)ramfs_close((int)fd);
}

/*
 * syscall_dispatch() — 系统调用分派
 *
 * 由 trap_handler 在 ECALL 异常时调用。
 * 从 trap frame 中读取 a7(系统调用号) 和 a0-a2(参数)，
 * 分派到对应的处理函数，将返回值写回 a0。
 */
void syscall_dispatch(uint64_t *tf) {
    uint64_t nr = tf[17];  /* a7 = 系统调用号 */
    uint64_t arg0 = tf[10]; /* a0 = 参数1 / 返回值 */
    uint64_t arg1 = tf[11]; /* a1 = 参数2 */
    uint64_t arg2 = tf[12]; /* a2 = 参数3 */

    switch (nr) {
        case SYS_WRITE:
            tf[10] = sys_write(arg0, arg1, arg2);  /* 返回值写入 a0 */
            break;
        case SYS_READ:
            tf[10] = sys_read(arg0, arg1, arg2);    /* 返回值写入 a0 */
            break;
        case SYS_EXIT:
            sys_exit(arg0);
            break;
        case SYS_WAIT:
            tf[10] = sys_wait(arg0);
            break;
        case SYS_OPEN:
            tf[10] = sys_open(arg0, arg1);
            break;
        case SYS_CLOSE:
            tf[10] = sys_close(arg0);
            break;
        default:
            printk("Unknown syscall number: %ld\n", (long)nr);
            tf[10] = (uint64_t)-1;  /* 返回 -1 表示错误 */
            break;
    }
}
