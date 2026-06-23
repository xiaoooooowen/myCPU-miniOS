#include "trap.h"
#include "printk.h"
#include "timer.h"
#include "task.h"
#include "syscall.h"
#include "../include/csr.h"

/* 静默模式：抢占式调度阶段减少 trap 输出噪音 */
static int trap_silent = 0;

void trap_handler(uint64_t *tf) {
    uint64_t cause = trap_cause_read();
    uint64_t epc   = trap_epc_read();
    uint64_t tval  = trap_tval_read();
    int quiet_syscall = trap_silent && (cause == 8 || cause == 9);
    (void)tf;  /* 作为 trap frame 基址传递给 sched_tick */

    /* 判断是异常还是中断 */
    if (cause & (1ULL << 63)) {
        /* 中断 */
        uint64_t irq_code = cause & 0x7fffffffffffffffULL;

        if (!trap_silent) {
            printk("=== TRAP ===\n");
            printk("scause: %lx\n", cause);
            printk("sepc:   %lx\n", epc);
            printk("stval:  %lx\n", tval);
        }

        switch (irq_code) {
            case 5:
                if (!trap_silent) {
                    printk("Type: Interrupt (%lx)\n", cause);
                    printk("  -> Supervisor timer interrupt\n");
                }
                timer_handle();
                sched_tick(tf);
                if (!trap_silent) {
                    printk("=== TRAP END ===\n");
                }
                task_prepare_trap_return();
                return;
            case 1:
                printk("Type: Interrupt (%lx)\n", cause);
                printk("  -> Supervisor software interrupt\n");
                break;
            case 9:
                printk("Type: Interrupt (%lx)\n", cause);
                printk("  -> Supervisor external interrupt\n");
                break;
            default:
                printk("Type: Interrupt (%lx)\n", cause);
                printk("  -> Unknown interrupt\n");
                break;
        }

        if (!trap_silent) {
            printk("=== TRAP END ===\n");
        }
        task_prepare_trap_return();
        return;
    }

    /* 异常 */
    if (!quiet_syscall) {
        printk("=== TRAP ===\n");
        printk("scause: %lx\n", cause);
        printk("sepc:   %lx\n", epc);
        printk("stval:  %lx\n", tval);
        printk("Type: Exception (%lx)\n", cause);
    }

    int rescheduled = 0;  /* 是否已通过 sched_tick 切换上下文 */
    int context_replaced = 0;

    switch (cause) {
        case 8:
            if (!quiet_syscall)
                printk("  -> Environment call from U-mode\n");
            context_replaced = syscall_dispatch(tf);
            if (task_current_state() == TASK_ZOMBIE ||
                task_current_state() == TASK_BLOCKED) {
                if (task_current_state() == TASK_BLOCKED &&
                    context_replaced == 2)
                    trap_epc_write(epc + 4);
                rescheduled = task_reschedule(tf);
            }
            break;
        case 9:
            if (!quiet_syscall)
                printk("  -> Environment call from S-mode\n");
            context_replaced = syscall_dispatch(tf);
            if (task_current_state() == TASK_ZOMBIE ||
                task_current_state() == TASK_BLOCKED) {
                if (task_current_state() == TASK_BLOCKED &&
                    context_replaced == 2)
                    trap_epc_write(epc + 4);
                rescheduled = task_reschedule(tf);
            }
            break;
        case 3:
            printk("  -> Breakpoint\n");
            break;
        case 2:
            printk("  -> Illegal instruction\n");
            break;
        default:
            printk("  -> Unknown exception\n");
            break;
    }

    if (!quiet_syscall)
        printk("=== TRAP END ===\n");

    /* 异常：推进 sepc 跳过触发异常的指令
     * 注意：若已通过 sched_tick 重调度，sepc 已被设置为新任务的入口，
     * 不应覆盖 */
    if (!rescheduled && !context_replaced) {
        trap_epc_write(epc + 4);
    }

    task_prepare_trap_return();
}

/* 进入抢占式调度阶段后，静默定时器中断的 trap 输出 */
void trap_set_silent(int silent) {
    trap_silent = silent;
}
