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
        return;
    }

    /* 异常 */
    printk("=== TRAP ===\n");
    printk("scause: %lx\n", cause);
    printk("sepc:   %lx\n", epc);
    printk("stval:  %lx\n", tval);
    printk("Type: Exception (%lx)\n", cause);

    int rescheduled = 0;  /* 是否已通过 sched_tick 切换上下文 */

    switch (cause) {
        case 8:
            printk("  -> Environment call from U-mode\n");
            syscall_dispatch(tf);
            if (task_current_state() == TASK_ZOMBIE) {
                sched_tick(tf);
                rescheduled = 1;
            }
            break;
        case 9:
            printk("  -> Environment call from S-mode\n");
            syscall_dispatch(tf);
            if (task_current_state() == TASK_ZOMBIE) {
                sched_tick(tf);
                rescheduled = 1;
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

    printk("=== TRAP END ===\n");

    /* 异常：推进 sepc 跳过触发异常的指令
     * 注意：若已通过 sched_tick 重调度，sepc 已被设置为新任务的入口，
     * 不应覆盖 */
    if (!rescheduled) {
        trap_epc_write(epc + 4);
    }

    /*
     * 若因 U 模式 ecall 退出而触发重调度，需恢复 SPP=1，
     * 确保下一个任务运行在 Supervisor 模式。
     * S 模式 ecall 退出时 SPP 已是 1，无需处理。
     */
    if (rescheduled && cause == 8) {
        uint64_t status = csr_read(sstatus);
        status |= SSTATUS_SPP;
        csr_write(sstatus, status);
    }
}

/* 进入抢占式调度阶段后，静默定时器中断的 trap 输出 */
void trap_set_silent(int silent) {
    trap_silent = silent;
}
