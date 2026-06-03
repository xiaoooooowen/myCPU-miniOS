#include "task.h"
#include "mem.h"
#include "printk.h"
#include "../include/csr.h"

/* 任务表 */
static struct task tasks[MAX_TASKS];
static struct task *current = NULL;
static int task_count = 0;

/*
 * 简单轮询调度器：找到下一个 READY 任务
 */
static struct task *schedule(void) {
    if (task_count == 0)
        return NULL;

    int idx = 0;
    if (current != NULL) {
        /* 从 current 的下一个开始找 */
        idx = (current - tasks + 1) % MAX_TASKS;
    }

    for (int i = 0; i < MAX_TASKS; i++) {
        int tid = (idx + i) % MAX_TASKS;
        if (tasks[tid].state == TASK_READY ||
            tasks[tid].state == TASK_RUNNING) {
            return &tasks[tid];
        }
    }
    return NULL;
}

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
    }
    task_count = 0;

    /* task[0] 为初始 boot 任务，当前正在执行 */
    tasks[0].state = TASK_RUNNING;
    tasks[0].name  = "idle";
    tasks[0].stack = NULL;  /* 使用启动栈 _stack_top */
    current = &tasks[0];
    task_count = 1;

    printk("Task subsystem initialized (idle task as task[0]).\n");
}

int task_create(void (*entry)(void), const char *name) {
    if (task_count >= MAX_TASKS)
        return -1;

    /* 找一个空闲槽位 */
    int tid = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            tid = i;
            break;
        }
    }
    if (tid < 0)
        return -1;

    struct task *t = &tasks[tid];

    /* 分配内核栈页 */
    t->stack = kalloc();
    if (t->stack == NULL)
        return -1;

    /*
     * 初始化上下文：
     *   sp 指向栈顶（栈从高地址向低地址增长）
     *   ra 指向任务入口函数
     *   callee-saved 清零
     */
    t->ctx.ra  = (uint64_t)entry;
    t->ctx.sp  = (uint64_t)t->stack + PAGE_SIZE;
    t->ctx.s0  = 0;
    t->ctx.s1  = 0;
    t->ctx.s2  = 0;
    t->ctx.s3  = 0;
    t->ctx.s4  = 0;
    t->ctx.s5  = 0;
    t->ctx.s6  = 0;
    t->ctx.s7  = 0;
    t->ctx.s8  = 0;
    t->ctx.s9  = 0;
    t->ctx.s10 = 0;
    t->ctx.s11 = 0;

    t->state = TASK_READY;
    t->name  = name;
    task_count++;

    printk("Created task '%s' (tid=%d, stack=%lx, entry=%lx)\n",
           t->name, tid, (uint64_t)t->stack, (uint64_t)entry);
    return tid;
}

void yield(void) {
    if (current == NULL || task_count <= 1)
        return;

    struct task *prev = current;
    struct task *next = schedule();

    if (next == NULL || next == prev)
        return;

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current = next;

    switch_to(&prev->ctx, &next->ctx);
}

/*
 * sched_tick() — 抢占式调度入口
 *
 * 由 trap_handler() 在定时器中断时调用。
 * 运行在 trap_entry 保存的栈帧之上（sp 指向 trap frame 底部）。
 *
 * 核心逻辑：
 *   1. 从 trap frame 中提取当前任务的 callee-saved 寄存器，保存到 current->ctx
 *   2. 调用 schedule() 选择下一个就绪任务
 *   3. 将下一个任务的 ctx 写回 trap frame 和 mepc
 *   4. trap_entry 的尾随后续恢复寄存器 + mret 时，
 *      将自然跳转到新任务的执行流
 *
 * trap frame 布局（trap.S 定义，256 字节）：
 *   tf[0 ] = sp+0   未使用（x0 硬连线零）
 *   tf[1 ] = sp+8   x1  (ra)
 *   tf[2 ] = sp+16  x2  (sp_original) — 进入 trap 前的 sp
 *   tf[3 ] = sp+24  x3  (gp)
 *   tf[4 ] = sp+32  x4  (tp)
 *   tf[5 ] = sp+40  x5  (t0)
 *   tf[6 ] = sp+48  x6  (t1)
 *   tf[7 ] = sp+56  x7  (t2)
 *   tf[8 ] = sp+64  x8  (s0/fp)
 *   tf[9 ] = sp+72  x9  (s1)
 *   tf[10] = sp+80  x10 (a0)
 *   tf[11] = sp+88  x11 (a1)
 *   tf[12] = sp+96  x12 (a2)
 *   tf[13] = sp+104 x13 (a3)
 *   tf[14] = sp+112 x14 (a4)
 *   tf[15] = sp+120 x15 (a5)
 *   tf[16] = sp+128 x16 (a6)
 *   tf[17] = sp+136 x17 (a7)
 *   tf[18] = sp+144 x18 (s2)
 *   tf[19] = sp+152 x19 (s3)
 *   tf[20] = sp+160 x20 (s4)
 *   tf[21] = sp+168 x21 (s5)
 *   tf[22] = sp+176 x22 (s6)
 *   tf[23] = sp+184 x23 (s7)
 *   tf[24] = sp+192 x24 (s8)
 *   tf[25] = sp+200 x25 (s9)
 *   tf[26] = sp+208 x26 (s10)
 *   tf[27] = sp+216 x27 (s11)
 *   tf[28] = sp+224 x28 (t3)
 *   tf[29] = sp+232 x29 (t4)
 *   tf[30] = sp+240 x30 (t5)
 *   tf[31] = sp+248 x31 (t6)
 */
void sched_tick(uint64_t *tf) {
    if (current == NULL || task_count <= 1)
        return;

    /* tf 由 trap_entry 通过 trap_handler 传入，指向栈上的 trap frame 基址 */

    /*
     * 保存当前任务上下文：
     *   ra  = mepc（被中断的指令地址，mret 后应返回此处）
     *   sp  = tf[2]（进入 trap 前的原始 sp）
     *   s0-s11 直接从 trap frame 中读取
     *
     * 注意：caller-saved 寄存器（ra/a0-a7/t0-t6）不需要保存，
     * trap_entry 已经把它们保存在 trap frame 中，后续会由尾随恢复。
     */
    current->ctx.ra  = trap_epc_read();
    current->ctx.sp  = tf[2];
    current->ctx.s0  = tf[8];
    current->ctx.s1  = tf[9];
    current->ctx.s2  = tf[18];
    current->ctx.s3  = tf[19];
    current->ctx.s4  = tf[20];
    current->ctx.s5  = tf[21];
    current->ctx.s6  = tf[22];
    current->ctx.s7  = tf[23];
    current->ctx.s8  = tf[24];
    current->ctx.s9  = tf[25];
    current->ctx.s10 = tf[26];
    current->ctx.s11 = tf[27];

    struct task *next = schedule();
    if (next == NULL || next == current)
        return;

    if (current->state == TASK_RUNNING)
        current->state = TASK_READY;
    next->state = TASK_RUNNING;

    /*
     * 恢复下一个任务上下文：
     *   1. 将 next->ctx 中的 callee-saved 寄存器写回 trap frame
     *   2. 将 mepc 设为 next->ctx.ra（mret 时将跳转到此处）
     *
     * 对于新创建的任务：ctx.ra = entry, ctx.sp = 栈顶, ctx.s*=0
     *   → trap_entry 尾随恢复 sp 到新任务栈 + mret 跳转到 entry ✓
     *
     * 对于被抢占的任务：ctx 保存着被中断时的完整 callee-saved 状态
     *   → trap_entry 尾随恢复 sp 到任务原栈 + mret 跳转到被中断指令 ✓
     */
    trap_epc_write(next->ctx.ra);
    tf[2]  = next->ctx.sp;
    tf[8]  = next->ctx.s0;
    tf[9]  = next->ctx.s1;
    tf[18] = next->ctx.s2;
    tf[19] = next->ctx.s3;
    tf[20] = next->ctx.s4;
    tf[21] = next->ctx.s5;
    tf[22] = next->ctx.s6;
    tf[23] = next->ctx.s7;
    tf[24] = next->ctx.s8;
    tf[25] = next->ctx.s9;
    tf[26] = next->ctx.s10;
    tf[27] = next->ctx.s11;

    current = next;
}
