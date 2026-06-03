#ifndef MINIOS_TASK_H
#define MINIOS_TASK_H

#include <stdint.h>

#define MAX_TASKS     8
#define TASK_NAME_LEN 16

/* 任务状态 */
enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
};

/*
 * 上下文帧：保存 callee-saved 寄存器
 *
 * RISC-V calling convention:
 *   Caller-saved (编译器负责): ra, t0-t6, a0-a7
 *   Callee-saved (必须手动保存): sp, s0-s11
 *
 * switch_to 只保存/恢复 callee-saved 寄存器。
 * 调用者寄存器由 C 编译器在函数调用的栈帧中自动处理。
 */
struct context {
    uint64_t ra;    /* x1  返回地址 */
    uint64_t sp;    /* x2  栈指针   */
    uint64_t s0;    /* x8  帧指针   */
    uint64_t s1;    /* x9           */
    uint64_t s2;    /* x18          */
    uint64_t s3;    /* x19          */
    uint64_t s4;    /* x20          */
    uint64_t s5;    /* x21          */
    uint64_t s6;    /* x22          */
    uint64_t s7;    /* x23          */
    uint64_t s8;    /* x24          */
    uint64_t s9;    /* x25          */
    uint64_t s10;   /* x26          */
    uint64_t s11;   /* x27          */
};

/* 任务控制块 */
struct task {
    struct context ctx;             /* callee-saved 上下文 */
    void          *stack;           /* 内核栈基址 (kalloc 分配的页) */
    int            state;           /* 任务状态 */
    const char    *name;            /* 调试名称 */
};

/* 初始化任务子系统 */
void task_init(void);

/* 创建一个新任务，返回任务索引，失败返回 -1 */
int  task_create(void (*entry)(void), const char *name);

/* 协作式让出 CPU */
void yield(void);

/* 抢占式调度入口（由定时器中断处理函数调用，接收 trap frame 基址） */
void sched_tick(uint64_t *trap_frame);

/* 上下文切换（汇编实现） */
void switch_to(struct context *prev, struct context *next);

#endif /* MINIOS_TASK_H */
