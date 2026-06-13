#include "timer.h"
#include "printk.h"
#include "../include/csr.h"

static volatile uint64_t *mtime = (volatile uint64_t *)CLINT_MTIME;
static volatile uint64_t *mtimecmp = (volatile uint64_t *)CLINT_MTIMECMP;
static unsigned int tick_count = 0;
static uint64_t timer_interval = TIMER_INTERVAL;
static unsigned int timeslice_ms = TIMER_DEFAULT_SLICE_MS;

void timer_init(void) {
    /* 设置首次定时器中断 */
    *mtimecmp = *mtime + timer_interval;

    /* 使能监管模式定时器中断（sie.STIE） */
    csr_set(sie, SIE_STIE);

    printk("Timer initialized: slice=%dms interval=%ld ticks\n",
           (int)timeslice_ms, (long)timer_interval);
}

void timer_handle(void) {
    tick_count++;

    /* 设置下一次定时器中断 */
    *mtimecmp = *mtime + timer_interval;
}

void timer_set_interval(uint64_t interval) {
    if (interval == 0)
        interval = 1;
    timer_interval = interval;
    *mtimecmp = *mtime + timer_interval;
}

void timer_set_timeslice_ms(unsigned int milliseconds) {
    if (milliseconds == 0)
        milliseconds = 1;
    timer_set_interval((uint64_t)milliseconds * TIMER_TICKS_PER_MS);
    timeslice_ms = milliseconds;
}

unsigned int timer_get_timeslice_ms(void) {
    return timeslice_ms;
}

uint64_t timer_get_interval(void) {
    return timer_interval;
}

unsigned int timer_get_ticks(void) {
    return tick_count;
}

uint64_t timer_now(void) {
    return *mtime;
}
