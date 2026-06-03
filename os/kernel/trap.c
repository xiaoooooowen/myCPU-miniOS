#include "trap.h"
#include "printk.h"
#include "../include/csr.h"

void trap_handler(void) {
    uint64_t cause = trap_cause_read();
    uint64_t epc   = trap_epc_read();
    uint64_t tval  = trap_tval_read();

    printk("=== TRAP HANDLER ===\n");
    printk("mcause: %lx\n", cause);
    printk("mepc:   %lx\n", epc);
    printk("mtval:  %lx\n", tval);

    /* 判断是异常还是中断 */
    if (cause & (1ULL << 63)) {
        /* 最高位为 1 表示中断 */
        uint64_t irq_code = cause & 0x7fffffffffffffffULL;
        printk("Type: Interrupt (%lx)\n", irq_code);
    } else {
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
                printk("  -> Unknown\n");
                break;
        }
    }

    printk("=== TRAP END ===\n");

    /* 将 mepc 推进到下一条指令，避免 mret 回到同一 ecall 形成死循环 */
    trap_epc_write(epc + 4);
}
