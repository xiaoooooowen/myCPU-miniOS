#ifndef MINIOS_TIMER_H
#define MINIOS_TIMER_H

/* CLINT MMIO 地址（机器模式定时器） */
#define CLINT_BASE      0x02000000UL
#define CLINT_MTIMECMP  (CLINT_BASE + 0x4000)
#define CLINT_MTIME     (CLINT_BASE + 0xbff8)

/* 每次中断的时间间隔（模拟器每条指令 tick 1 次） */
#define TIMER_INTERVAL  500

void timer_init(void);
void timer_handle(void);

#endif /* MINIOS_TIMER_H */
