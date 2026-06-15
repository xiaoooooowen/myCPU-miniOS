#ifndef MINIOS_SYSCALL_H
#define MINIOS_SYSCALL_H

#include <stdint.h>

#define SYS_DUP2     24
#define SYS_MKDIR    34
#define SYS_UNLINK   35
#define SYS_OPEN     56
#define SYS_CLOSE    57
#define SYS_GETDENTS 61
#define SYS_LSEEK    62
#define SYS_READ     63
#define SYS_WRITE    64
#define SYS_EXIT     93
#define SYS_WAIT     95
#define SYS_YIELD    124
#define SYS_FORK     220
#define SYS_EXECVE   221
#define SYS_WAITPID  260
#define SYS_PS       400
#define SYS_CHDIR    402
#define SYS_GETCWD   403
#define SYS_KILL     405

int syscall_dispatch(uint64_t *trap_frame);

#endif
