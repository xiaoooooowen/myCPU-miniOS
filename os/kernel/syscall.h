#ifndef MINIOS_SYSCALL_H
#define MINIOS_SYSCALL_H

#include <stdint.h>

/*
 * 系统调用号定义
 *
 * 使用 RISC-V Linux 约定：
 *   a7 = 系统调用号
 *   a0 = 参数1 / 返回值
 *   a1 = 参数2
 *   a2 = 参数3
 */

#define SYS_EXIT    93    /* 退出/停机 */
#define SYS_WRITE   64    /* 向控制台输出字符串 */

/*
 * syscall_dispatch() — 系统调用分派函数
 *
 * 由 trap_handler 在 ECALL 异常时调用。
 *
 * 参数：
 *   tf     trap frame 指针（用于读取/写回寄存器）
 *
 * tf 布局（与 trap.S 保持一致）：
 *   tf[10] = a0  (参数1 / 返回值)
 *   tf[11] = a1  (参数2)
 *   tf[12] = a2  (参数3)
 *   tf[17] = a7  (系统调用号)
 */
void syscall_dispatch(uint64_t *tf);

#endif /* MINIOS_SYSCALL_H */
