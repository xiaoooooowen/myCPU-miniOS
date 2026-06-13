#ifndef MINIOS_TIMER_H
#define MINIOS_TIMER_H

#include <stdint.h>

/* CLINT MMIO 地址（机器模式定时器） */
#define CLINT_BASE      0x02000000UL
#define CLINT_MTIMECMP  (CLINT_BASE + 0x4000)
#define CLINT_MTIME     (CLINT_BASE + 0xbff8)

/* 模拟时钟约定：5000 CLINT ticks = 1ms，默认时间片10ms。 */
#define TIMER_TICKS_PER_MS       5000UL
#define TIMER_DEFAULT_SLICE_MS   10U
#define TIMER_INTERVAL           \
    (TIMER_TICKS_PER_MS * TIMER_DEFAULT_SLICE_MS)

void timer_init(void);
void timer_handle(void);
void timer_set_interval(uint64_t interval);
uint64_t timer_get_interval(void);
void timer_set_timeslice_ms(unsigned int milliseconds);
unsigned int timer_get_timeslice_ms(void);
unsigned int timer_get_ticks(void);
uint64_t timer_now(void);

#endif /* MINIOS_TIMER_H */
