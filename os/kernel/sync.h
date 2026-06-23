#ifndef MINIOS_SYNC_H
#define MINIOS_SYNC_H

struct semaphore {
    int value;
};

struct mutex {
    struct semaphore sem;
    int owner;
};

void sem_init(struct semaphore *sem, int value);
void sem_wait(struct semaphore *sem);
void sem_post(struct semaphore *sem);

void mutex_init(struct mutex *mutex);
void mutex_lock(struct mutex *mutex);
int mutex_trylock(struct mutex *mutex);
void mutex_unlock(struct mutex *mutex);

#endif
