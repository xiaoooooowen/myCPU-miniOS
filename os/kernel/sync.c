#include "sync.h"
#include "task.h"
#include "../include/csr.h"

void sem_init(struct semaphore *sem, int value) {
    if (sem == 0)
        return;
    sem->value = value < 0 ? 0 : value;
}

void sem_wait(struct semaphore *sem) {
    if (sem == 0)
        return;

    while (1) {
        uint64_t irq_state = local_irq_save();
        if (sem->value > 0) {
            sem->value--;
            local_irq_restore(irq_state);
            return;
        }
        local_irq_restore(irq_state);
        task_block(sem);
    }
}

void sem_post(struct semaphore *sem) {
    if (sem == 0)
        return;

    uint64_t irq_state = local_irq_save();
    sem->value++;
    local_irq_restore(irq_state);
    task_wake_one(sem);
}

void mutex_init(struct mutex *mutex) {
    if (mutex == 0)
        return;
    sem_init(&mutex->sem, 1);
    mutex->owner = -1;
}

void mutex_lock(struct mutex *mutex) {
    if (mutex == 0)
        return;
    sem_wait(&mutex->sem);
    mutex->owner = task_current_pid();
}

int mutex_trylock(struct mutex *mutex) {
    if (mutex == 0)
        return 0;

    uint64_t irq_state = local_irq_save();
    if (mutex->sem.value == 0) {
        local_irq_restore(irq_state);
        return 0;
    }
    mutex->sem.value = 0;
    mutex->owner = task_current_pid();
    local_irq_restore(irq_state);
    return 1;
}

void mutex_unlock(struct mutex *mutex) {
    if (mutex == 0 || mutex->owner != task_current_pid())
        return;
    mutex->owner = -1;
    sem_post(&mutex->sem);
}
