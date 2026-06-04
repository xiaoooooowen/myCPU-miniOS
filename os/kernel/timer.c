#include "timer.h"
#include "printk.h"
#include "../include/csr.h"

static volatile uint64_t *mtime = (volatile uint64_t *)CLINT_MTIME;
static volatile uint64_t *mtimecmp = (volatile uint64_t *)CLINT_MTIMECMP;
static unsigned int tick_count = 0;

void timer_init(void) {
    /* 设置首次定时器中断 */
    *mtimecmp = *mtime + TIMER_INTERVAL;

    /* 使能监管模式定时器中断（sie.STIE） */
    csr_set(sie, SIE_STIE);

    printk("Timer initialized: mtime=%lx, mtimecmp=%lx\n",
           *mtime, *mtimecmp);
}

void timer_handle(void) {
    tick_count++;

    /* 设置下一次定时器中断 */
    *mtimecmp = *mtime + TIMER_INTERVAL;
}
