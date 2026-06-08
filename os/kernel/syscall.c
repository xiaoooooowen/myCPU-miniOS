#include "syscall.h"
#include "uart.h"
#include "printk.h"

/* TEST_FINISH 设备地址：向此地址写入任意值通知模拟器停止运行 */
#define TEST_FINISH 0x100000

/*
 * sys_write() — 向控制台输出字符串
 *
 * 参数：
 *   fd   (未使用，只支持控制台)
 *   buf  指向字符串缓冲区
 *   len  要输出的字节数
 *
 * 返回值：实际输出的字节数
 */
static uint64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    (void)fd;

    if (buf == 0 || len == 0)
        return 0;

    const char *s = (const char *)buf;
    uint64_t written = 0;
    for (uint64_t i = 0; i < len; i++) {
        uart_putc(s[i]);
        written++;
    }
    return written;
}

/*
 * sys_exit() — 退出/停机
 *
 * 进入死循环，停止系统推进。
 *
 * 参数：
 *   code  退出码（当前未使用）
 *
 * 返回值：无
 */
static void sys_exit(uint64_t code) {
    printk("\n--- System Exit (code=%ld) ---\n", (long)code);
    *(volatile uint32_t *)TEST_FINISH = 0x5555;
    while (1)
        ;
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
        case SYS_EXIT:
            sys_exit(arg0);
            break;
        default:
            printk("Unknown syscall number: %ld\n", (long)nr);
            tf[10] = (uint64_t)-1;  /* 返回 -1 表示错误 */
            break;
    }
}
