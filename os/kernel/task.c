#include "task.h"
#include "mem.h"
#include "printk.h"
#include "user.h"
#include "timer.h"
#include "../include/csr.h"

#ifndef MINIOS_BOOT_DIAGNOSTICS
#define MINIOS_BOOT_DIAGNOSTICS 0
#endif

static struct task tasks[MAX_TASKS];
static struct task *current = NULL;
static int task_count = 0;
static int next_pid = 1;
static uint64_t order_counter = 0;
static enum sched_policy scheduler_policy = SCHED_RR;
static unsigned int scheduler_quantum = TASK_DEFAULT_QUANTUM;
static uint64_t switch_samples = 0;
static uint64_t max_switch_ticks = 0;
extern char _stack_top;

static void copy_name(char *dst, const char *src) {
    int i = 0;
    if (src != NULL) {
        while (i < TASK_NAME_LEN - 1 && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static const char *state_name(int state) {
    switch (state) {
        case TASK_UNUSED:  return "UNUSED";
        case TASK_READY:   return "READY";
        case TASK_RUNNING: return "RUNNING";
        case TASK_BLOCKED: return "BLOCKED";
        case TASK_ZOMBIE:  return "ZOMBIE";
        default:           return "UNKNOWN";
    }
}

static struct task *find_pid(int pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].pid == pid)
            return &tasks[i];
    }
    return NULL;
}

static int has_ready_non_idle(void) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_READY)
            return 1;
    }
    return 0;
}

static struct task *select_rr(void) {
    int start = 0;
    if (current != NULL)
        start = (int)(current - tasks + 1) % MAX_TASKS;

    for (int i = 0; i < MAX_TASKS; i++) {
        struct task *candidate = &tasks[(start + i) % MAX_TASKS];
        if (candidate->state == TASK_READY)
            return candidate;
    }
    return NULL;
}

static struct task *select_fcfs(void) {
    struct task *best = NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_READY)
            continue;
        if (best == NULL || tasks[i].ready_order < best->ready_order)
            best = &tasks[i];
    }
    return best;
}

static struct task *select_next(void) {
    if (scheduler_policy == SCHED_FCFS)
        return select_fcfs();
    return select_rr();
}

static void save_trap_context(struct task *task, uint64_t *tf) {
    for (int i = 0; i < TASK_REG_COUNT; i++)
        task->trap_ctx.regs[i] = tf[i];
    task->trap_ctx.epc = trap_epc_read();
    task->trap_ctx.status = trap_status_read();
    task->trap_ctx.satp = csr_read(satp);
}

static void restore_trap_context(const struct task *task, uint64_t *tf) {
    for (int i = 0; i < TASK_REG_COUNT; i++)
        tf[i] = task->trap_ctx.regs[i];
    trap_epc_write(task->trap_ctx.epc);
    trap_status_write(task->trap_ctx.status);
    trap_scratch_write(task->stack == NULL
        ? (uint64_t)&_stack_top
        : (uint64_t)task->stack + PAGE_SIZE);
    if (csr_read(satp) != task->trap_ctx.satp) {
        csr_write(satp, task->trap_ctx.satp);
        __asm__ volatile("sfence.vma x0, x0");
    }
}

static int switch_from_trap(uint64_t *tf, int force) {
    if (current == NULL || task_count <= 1)
        return 0;

    save_trap_context(current, tf);

    if (!force) {
        current->runtime_ticks++;

        if (scheduler_policy == SCHED_FCFS &&
            current->pid != 0 && current->state == TASK_RUNNING)
            return 0;

        if (scheduler_policy == SCHED_RR &&
            current->state == TASK_RUNNING && current->pid != 0) {
            if (current->ticks_left > 1) {
                current->ticks_left--;
                return 0;
            }
        }

        if (current->pid == 0 && !has_ready_non_idle())
            return 0;
    }

    struct task *prev = current;
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
        prev->ready_order = ++order_counter;
    }

    struct task *next = select_next();
    if (next == NULL) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            prev->ticks_left = prev->time_slice;
        }
        return 0;
    }

    next->state = TASK_RUNNING;
    next->ticks_left = next->time_slice;
    next->context_switches++;
    current = next;
    restore_trap_context(next, tf);
    return next != prev;
}

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].pid = -1;
        tasks[i].ppid = -1;
        tasks[i].stack = NULL;
        tasks[i].has_user_space = 0;
        tasks[i].name[0] = '\0';
    }

    task_count = 1;
    next_pid = 1;
    order_counter = 0;
    switch_samples = 0;
    max_switch_ticks = 0;

    tasks[0].state = TASK_RUNNING;
    tasks[0].pid = 0;
    tasks[0].ppid = -1;
    tasks[0].stack = NULL;
    tasks[0].time_slice = scheduler_quantum;
    tasks[0].ticks_left = scheduler_quantum;
    tasks[0].created_order = ++order_counter;
    tasks[0].ready_order = tasks[0].created_order;
    tasks[0].trap_ctx.satp = csr_read(satp);
    tasks[0].trap_ctx.status = trap_status_read() | SSTATUS_SPP;
    copy_name(tasks[0].name, "idle");
    current = &tasks[0];

#if MINIOS_BOOT_DIAGNOSTICS
    printk("Process subsystem initialized: policy=%s quantum=%d tick(s)\n",
           task_scheduler_name(), (int)scheduler_quantum);
#endif
}

int task_create(void (*entry)(void), const char *name) {
    if (entry == NULL || task_count >= MAX_TASKS)
        return -1;

    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;

    struct task *task = &tasks[slot];
    void *stack = kalloc();
    if (stack == NULL)
        return -1;

    for (int i = 0; i < TASK_REG_COUNT; i++)
        task->trap_ctx.regs[i] = 0;

    task->stack = stack;
    task->pid = next_pid++;
    task->ppid = current != NULL ? current->pid : -1;
    task->state = TASK_READY;
    task->exit_code = 0;
    task->wait_target = -1;
    task->wait_channel = NULL;
    task->time_slice = scheduler_quantum;
    task->ticks_left = scheduler_quantum;
    task->created_order = ++order_counter;
    task->ready_order = task->created_order;
    task->runtime_ticks = 0;
    task->context_switches = 0;
    task->has_user_space = 0;
    copy_name(task->name, name);

    task->ctx.ra = (uint64_t)entry;
    task->ctx.sp = (uint64_t)stack + PAGE_SIZE;
    task->trap_ctx.regs[2] = task->ctx.sp;
    task->trap_ctx.epc = (uint64_t)entry;
    task->trap_ctx.status = trap_status_read() | SSTATUS_SPP | SSTATUS_SPIE;
    task->trap_ctx.satp = csr_read(satp);
    task_count++;

#if MINIOS_BOOT_DIAGNOSTICS
    printk("Created process '%s' pid=%d ppid=%d stack=%lx entry=%lx\n",
           task->name, task->pid, task->ppid,
           (uint64_t)task->stack, (uint64_t)entry);
#endif
    return task->pid;
}

int task_fork_from_trap(uint64_t *tf, uint64_t child_epc,
                        const struct task_address_space *address_space) {
    if (current == NULL || tf == NULL || address_space == NULL ||
        task_count >= MAX_TASKS)
        return -1;

    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;

    void *kernel_stack = kalloc();
    if (kernel_stack == NULL)
        return -1;

    struct task *child = &tasks[slot];
    for (int i = 0; i < TASK_REG_COUNT; i++)
        child->trap_ctx.regs[i] = tf[i];

    child->stack = kernel_stack;
    child->pid = next_pid++;
    child->ppid = current->pid;
    child->state = TASK_READY;
    child->exit_code = 0;
    child->wait_target = -1;
    child->wait_channel = NULL;
    child->time_slice = scheduler_quantum;
    child->ticks_left = scheduler_quantum;
    child->created_order = ++order_counter;
    child->ready_order = child->created_order;
    child->runtime_ticks = 0;
    child->context_switches = 0;
    child->has_user_space = 1;
    child->address_space = *address_space;
    copy_name(child->name, "fork-child");

    child->trap_ctx.regs[10] = 0; /* fork 在子进程返回 0 */
    child->trap_ctx.epc = child_epc;
    child->trap_ctx.status = trap_status_read() & ~SSTATUS_SPP;
    child->trap_ctx.satp = address_space->satp;
    task_count++;
    printk("fork: parent=%d child=%d (independent satp=%lx)\n",
           current->pid, child->pid, child->trap_ctx.satp);
    task_dump_tree();
    return child->pid;
}

int task_attach_address_space(const struct task_address_space *address_space) {
    if (current == NULL || address_space == NULL)
        return -1;
    current->address_space = *address_space;
    current->has_user_space = 1;
    current->trap_ctx.satp = address_space->satp;
    return 0;
}

struct task_address_space *task_current_address_space(void) {
    if (current == NULL || !current->has_user_space)
        return NULL;
    return &current->address_space;
}

void yield(void) {
    if (current == NULL || task_count <= 1)
        return;

    struct task *prev = current;
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
        prev->ready_order = ++order_counter;
    }

    struct task *next = select_next();
    if (next == NULL || next == prev) {
        prev->state = TASK_RUNNING;
        return;
    }

    next->state = TASK_RUNNING;
    next->ticks_left = next->time_slice;
    next->context_switches++;
    current = next;
    switch_to(&prev->ctx, &next->ctx);
}

int sched_tick(uint64_t *tf) {
    uint64_t start = timer_now();
    int switched = switch_from_trap(tf, 0);
    if (switched) {
        uint64_t elapsed = timer_now() - start;
        switch_samples++;
        if (elapsed > max_switch_ticks)
            max_switch_ticks = elapsed;
    }
    return switched;
}

int task_reschedule(uint64_t *tf) {
    uint64_t start = timer_now();
    int switched = switch_from_trap(tf, 1);
    if (switched) {
        uint64_t elapsed = timer_now() - start;
        switch_samples++;
        if (elapsed > max_switch_ticks)
            max_switch_ticks = elapsed;
    }
    return switched;
}

void task_exit(int exit_code) {
    if (current == NULL || current->pid == 0)
        return;
    current->exit_code = exit_code;
    current->state = TASK_ZOMBIE;

    struct task *parent = find_pid(current->ppid);
    if (parent != NULL && parent->state == TASK_BLOCKED &&
        (parent->wait_target < 0 || parent->wait_target == current->pid)) {
        parent->state = TASK_READY;
        parent->wait_channel = NULL;
        parent->ready_order = ++order_counter;
    }

    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].ppid == current->pid)
            tasks[i].ppid = 0;
    }
}

int task_waitpid(int pid, int *status, int nohang) {
    if (current == NULL)
        return -1;

    int has_child = 0;
    for (int i = 1; i < MAX_TASKS; i++) {
        struct task *child = &tasks[i];
        if (child->state == TASK_UNUSED || child->ppid != current->pid)
            continue;
        if (pid > 0 && child->pid != pid)
            continue;

        has_child = 1;
        if (child->state != TASK_ZOMBIE)
            continue;

        int child_pid = child->pid;
        if (status != NULL)
            *status = child->exit_code;
        printk("waitpid: parent=%d reaped child=%d exit=%d\n",
               current->pid, child_pid, child->exit_code);
        if (child->stack != NULL)
            kfree(child->stack);
        if (child->has_user_space)
            user_space_destroy(&child->address_space);

        child->stack = NULL;
        child->has_user_space = 0;
        child->state = TASK_UNUSED;
        child->pid = -1;
        child->ppid = -1;
        child->name[0] = '\0';
        task_count--;
        return child_pid;
    }

    if (!has_child)
        return -1;
    if (nohang)
        return 0;

    current->wait_target = pid;
    current->wait_channel = current;
    current->state = TASK_BLOCKED;
    return TASK_WAIT_BLOCKED;
}

int task_wait(void) {
    int pid = task_waitpid(-1, NULL, 1);
    return pid == 0 ? -1 : pid;
}

int task_current_state(void) {
    return current == NULL ? TASK_UNUSED : current->state;
}

int task_current_pid(void) {
    return current == NULL ? -1 : current->pid;
}

uint64_t task_current_kernel_stack_top(void) {
    if (current == NULL || current->stack == NULL)
        return (uint64_t)&_stack_top;
    return (uint64_t)current->stack + PAGE_SIZE;
}

void task_prepare_trap_return(void) {
    trap_scratch_write(task_current_kernel_stack_top());
}

void task_block(const void *channel) {
    if (current == NULL || current->pid == 0)
        return;

    uint64_t irq_state = local_irq_save();
    current->wait_channel = channel;
    current->state = TASK_BLOCKED;
    local_irq_restore(irq_state);
#ifdef __riscv
    __asm__ volatile(
        "li a7, 124\n"
        "ecall\n"
        :
        :
        : "a7"
    );
#endif
}

int task_wake_one(const void *channel) {
    uint64_t irq_state = local_irq_save();
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            tasks[i].wait_channel == channel) {
            tasks[i].state = TASK_READY;
            tasks[i].wait_channel = NULL;
            tasks[i].ready_order = ++order_counter;
            local_irq_restore(irq_state);
            return tasks[i].pid;
        }
    }
    local_irq_restore(irq_state);
    return -1;
}

int task_wake_all(const void *channel) {
    int count = 0;
    uint64_t irq_state = local_irq_save();
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            tasks[i].wait_channel == channel) {
            tasks[i].state = TASK_READY;
            tasks[i].wait_channel = NULL;
            tasks[i].ready_order = ++order_counter;
            count++;
        }
    }
    local_irq_restore(irq_state);
    return count;
}

void task_set_scheduler(enum sched_policy policy) {
    if (policy != SCHED_FCFS && policy != SCHED_RR)
        return;
    scheduler_policy = policy;
}

enum sched_policy task_get_scheduler(void) {
    return scheduler_policy;
}

const char *task_scheduler_name(void) {
    return scheduler_policy == SCHED_FCFS ? "FCFS" : "RR";
}

void task_set_quantum(unsigned int ticks) {
    if (ticks == 0)
        ticks = 1;
    scheduler_quantum = ticks;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED) {
            tasks[i].time_slice = ticks;
            tasks[i].ticks_left = ticks;
        }
    }
}

unsigned int task_get_quantum(void) {
    return scheduler_quantum;
}

void task_dump_processes(void) {
    printk("PID  PPID STATE    TICKS SWITCH NAME\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED)
            continue;
        printk("%d    %d    %s  %ld  %ld  %s\n",
               tasks[i].pid, tasks[i].ppid, state_name(tasks[i].state),
               (long)tasks[i].runtime_ticks,
               (long)tasks[i].context_switches, tasks[i].name);
    }
    printk("[%s] context switch max=%ld ticks, limit=%ld ticks (1ms), samples=%ld\n",
           max_switch_ticks < TIMER_TICKS_PER_MS ? "PASS" : "FAIL",
           (long)max_switch_ticks, (long)TIMER_TICKS_PER_MS,
           (long)switch_samples);
}

static void dump_tree_node(int pid, int depth) {
    struct task *task = find_pid(pid);
    if (task == NULL)
        return;

    for (int i = 0; i < depth; i++)
        printk("  ");
    printk("%s(%d) [%s]\n", task->name, task->pid, state_name(task->state));

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].ppid == pid)
            dump_tree_node(tasks[i].pid, depth + 1);
    }
}

void task_dump_tree(void) {
    printk("Process tree:\n");
    dump_tree_node(0, 0);
}
