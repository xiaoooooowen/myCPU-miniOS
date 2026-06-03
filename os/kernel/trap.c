#include "trap.h"
#include "printk.h"
#include "timer.h"
#include "../include/csr.h"

void trap_handler(void) {
    uint64_t cause = trap_cause_read();
    uint64_t epc   = trap_epc_read();
    uint64_t tval  = trap_tval_read();

    printk("=== TRAP ===\n");
    printk("mcause: %lx\n", cause);
    printk("mepc:   %lx\n", epc);
    printk("mtval:  %lx\n", tval);

    /* 判断是异常还是中断 */
    if (cause & (1ULL << 63)) {
        /* 最高位为 1 表示中断 */
        uint64_t irq_code = cause & 0x7fffffffffffffffULL;
        printk("Type: Interrupt (%lx)\n", irq_code);

        switch (irq_code) {
            case 7:
                printk("  -> Machine timer interrupt\n");
                timer_handle();
                break;
            case 3:
                printk("  -> Machine software interrupt\n");
                break;
            case 11:
                printk("  -> Machine external interrupt\n");
                break;
            default:
                printk("  -> Unknown interrupt\n");
                break;
        }

        /* 中断：mret 返回被中断的指令，不推进 mepc */
        printk("=== TRAP END ===\n");
        return;
    }

    /* 异常 */
    printk("Type: Exception (%lx)\n", cause);
    switch (cause) {
        case 10:
            printk("  -> Environment call from M-mode\n");
            break;
        case 9:
            printk("  -> Environment call from S-mode\n");
            break;
        case 8:
            printk("  -> Environment call from U-mode\n");
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

    /* 异常：推进 mepc 跳过触发异常的指令 */
    trap_epc_write(epc + 4);
}
