#ifndef MINIOS_TASK_H
#define MINIOS_TASK_H

#include <stdint.h>

#define MAX_TASKS     16
#define TASK_NAME_LEN 16
#define TASK_REG_COUNT 32
#define TASK_DEFAULT_QUANTUM 1
#define TASK_WAIT_BLOCKED (-2)

/* 任务状态 */
enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE,
};

enum sched_policy {
    SCHED_FCFS = 0,
    SCHED_RR,
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

/* Trap 路径使用的完整处理器上下文。 */
struct trap_context {
    uint64_t regs[TASK_REG_COUNT];
    uint64_t epc;
    uint64_t status;
    uint64_t satp;
};

struct task_address_space {
    void *root;
    void *l1_mmio;
    void *l0_user;
    void *text_page;
    void *stack_page;
    uint64_t satp;
};

/* 进程控制块（PCB）。 */
struct task {
    struct context ctx;             /* callee-saved 上下文 */
    struct trap_context trap_ctx;   /* 抢占/异常时的完整上下文 */
    void          *stack;           /* 内核栈基址 (kalloc 分配的页) */
    int            state;           /* 任务状态 */
    int            pid;
    int            ppid;
    int            exit_code;
    int            wait_target;
    const void    *wait_channel;
    unsigned int   time_slice;
    unsigned int   ticks_left;
    uint64_t       created_order;
    uint64_t       ready_order;
    uint64_t       runtime_ticks;
    uint64_t       context_switches;
    int            has_user_space;
    struct task_address_space address_space;
    char           name[TASK_NAME_LEN];
};

/* 初始化任务子系统 */
void task_init(void);

/* 创建一个新任务，返回任务索引，失败返回 -1 */
int  task_create(void (*entry)(void), const char *name);
int  task_fork_from_trap(uint64_t *trap_frame, uint64_t child_epc,
                         const struct task_address_space *address_space);
int  task_attach_address_space(const struct task_address_space *address_space);
struct task_address_space *task_current_address_space(void);

/* 协作式让出 CPU */
void yield(void);

/* 抢占式调度入口（由定时器中断处理函数调用，接收 trap frame 基址） */
int  sched_tick(uint64_t *trap_frame);

/* 当前进程退出或阻塞后，从 trap 上下文强制选择其他进程。 */
int  task_reschedule(uint64_t *trap_frame);

/* 上下文切换（汇编实现） */
void switch_to(struct context *prev, struct context *next);

/* 将当前任务标记为 ZOMBIE（由 sys_exit 调用） */
void task_exit(int exit_code);

/* 回收一个 ZOMBIE 子进程，返回其 pid，没有则返回 -1。 */
int  task_wait(void);
int  task_waitpid(int pid, int *status, int nohang);

/* 获取当前任务状态（由 trap_handler 检查是否需要重调度） */
int  task_current_state(void);
int  task_current_pid(void);
uint64_t task_current_kernel_stack_top(void);
void task_prepare_trap_return(void);
void task_block(const void *channel);
int  task_wake_one(const void *channel);
int  task_wake_all(const void *channel);

void task_set_scheduler(enum sched_policy policy);
enum sched_policy task_get_scheduler(void);
const char *task_scheduler_name(void);
void task_set_quantum(unsigned int ticks);
unsigned int task_get_quantum(void);
void task_dump_processes(void);
void task_dump_tree(void);

#endif /* MINIOS_TASK_H */
