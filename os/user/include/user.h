#ifndef MINIOS_USER_LIB_H
#define MINIOS_USER_LIB_H

#include <stddef.h>
#include <stdint.h>

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR   0x002
#define O_CREATE 0x040
#define O_TRUNC  0x200
#define O_APPEND 0x400

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define WNOHANG 1

#define MODE_FILE 1
#define MODE_DIR  2
#define MODE_EXEC 0x100

struct dirent_info {
    uint32_t inode;
    uint32_t mode;
    char name[56];
};

struct process_info {
    int pid;
    int ppid;
    int state;
    uint64_t runtime_ticks;
    uint64_t context_switches;
    char name[16];
};

long read(int fd, void *buffer, size_t length);
long write(int fd, const void *buffer, size_t length);
int open(const char *path, int flags);
int close(int fd);
long lseek(int fd, long offset, int whence);
int dup2(int oldfd, int newfd);
int mkdir(const char *path);
int unlink(const char *path);
int getdents(const char *path, struct dirent_info *entries, int capacity);
int chdir(const char *path);
int getcwd(char *buffer, int length);
int fork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int waitpid(int pid, int *status, int options);
int kill(int pid, int signal);
int set_cloexec(int fd, int on);
int getprocs(struct process_info *entries, int capacity);
void exit(int status) __attribute__((noreturn));

size_t strlen(const char *string);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t length);
char *strcpy(char *destination, const char *source);
char *strcat(char *destination, const char *source);
void *memset(void *destination, int value, size_t length);
void *memcpy(void *destination, const void *source, size_t length);
int atoi(const char *string);
char *getenv_from(char *const envp[], const char *name);

int putchar(int character);
int puts(const char *string);
int printf(const char *format, ...);

#endif
