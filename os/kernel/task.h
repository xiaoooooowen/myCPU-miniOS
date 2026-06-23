#ifndef MINIOS_TASK_H
#define MINIOS_TASK_H

#include <stdint.h>

#define MAX_TASKS     16
#define TASK_NAME_LEN 16
#define TASK_REG_COUNT 32
#define TASK_DEFAULT_QUANTUM 1
#define TASK_WAIT_BLOCKED (-2)
#define USER_MAX_PAGES 64
#define TASK_KERNEL_STACK_SIZE (4 * 4096)

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

struct user_page_mapping {
    uint64_t va;
    void *page;
    uint64_t flags;
};

struct task_address_space {
    void *root;
    void *l1_mmio;
    void *l0_user;
    struct user_page_mapping pages[USER_MAX_PAGES];
    uint32_t page_count;
    uint64_t satp;
};

/* 进程控制块（PCB）。 */
struct task {
    struct context ctx;             /* callee-saved 上下文（switch_to 切换用） */
    struct trap_context trap_ctx;   /* 抢占/异常时的完整上下文（trap 路径） */
    void          *stack;           /* 内核栈基址 (kalloc 分配的页) */
    int            state;           /* 任务状态：UNUSED/READY/RUNNING/BLOCKED/ZOMBIE */
    int            pid;             /* 进程 ID */
    int            ppid;            /* 父进程 ID */
    int            exit_code;       /* 退出码（ZOMBIE 时保留，供父进程 wait 读取） */
    int            wait_target;     /* 等待目标进程的 pid，-1 表示等待任意子进程 */
    const void    *wait_channel;    /* 阻塞等待的信道地址（用于同步/wake 机制） */
    unsigned int   time_slice;      /* 时间片大小（定时器 tick 数） */
    unsigned int   ticks_left;      /* 本轮剩余 tick 数，减到 0 触发抢占 */
    uint64_t       created_order;   /* 创建顺序（用于 FCFS 调度时确定优先级） */
    uint64_t       ready_order;     /* 就绪顺序（每次就绪时更新，按创建时间排序） */
    uint64_t       runtime_ticks;   /* 累计运行 tick 数 */
    uint64_t       context_switches;/* 累计上下文切换次数 */
    uint32_t       cwd_inode;       /* 当前工作目录的 inode 号 */
    int            has_user_space;  /* 是否拥有用户地址空间（内核线程为 0） */
    struct task_address_space address_space; /* 用户地址空间（页表、映射信息） */
    char           name[TASK_NAME_LEN];      /* 进程名（用于调试/ps 查看） */
};

struct task_info {
    int pid;
    int ppid;
    int state;
    uint64_t runtime_ticks;
    uint64_t context_switches;
    char name[TASK_NAME_LEN];
};

/* 初始化任务子系统 */
void task_init(void);

/* 创建一个新任务，返回任务索引，失败返回 -1 */
int  task_create(void (*entry)(void), const char *name);
int  task_fork_from_trap(uint64_t *trap_frame, uint64_t child_epc,
                         const struct task_address_space *address_space);
int  task_attach_address_space(const struct task_address_space *address_space);
int  task_replace_address_space(const struct task_address_space *address_space,
                                struct task_address_space *old_space);
struct task_address_space *task_current_address_space(void);
uint64_t *task_current_initial_trap_context(void);
void task_set_current_entry(uint64_t entry);
uint64_t task_current_entry(void);

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
uint32_t task_current_cwd(void);
void task_set_current_cwd(uint32_t inode);
void task_set_current_name(const char *name);
int task_kill(int pid, int exit_code);
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
int task_get_processes(struct task_info *entries, int capacity);

#endif /* MINIOS_TASK_H */
