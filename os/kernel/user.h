#ifndef MINIOS_USER_H
#define MINIOS_USER_H

#include <stdint.h>
#include "task.h"

#define USER_MIN_VA       0x10000ULL
#define USER_STACK_TOP    0x200000ULL
#define USER_STACK_PAGES  4
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_PAGES * 4096ULL)
#define USER_ARG_MAX      16
#define USER_ENV_MAX      8
#define USER_STRINGS_MAX  2048

int user_init(void);
int user_space_clone(struct task_address_space *dst,
                     const struct task_address_space *src);
void user_space_destroy(struct task_address_space *space);
int user_execve(uint64_t *trap_frame, const char *path,
                const char *const argv[], const char *const envp[]);

int copy_from_user(void *destination, uint64_t source, uint64_t length);
int copy_to_user(uint64_t destination, const void *source, uint64_t length);
int copy_string_from_user(char *destination, uint64_t source,
                          uint64_t capacity);

void enter_user(void);

#endif
