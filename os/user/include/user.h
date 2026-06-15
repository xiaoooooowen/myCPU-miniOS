#ifndef MINIOS_USER_LIB_H
#define MINIOS_USER_LIB_H

#include <stddef.h>
#include <stdint.h>

#ifndef MINIOS_BOOT_COLOR
#define MINIOS_BOOT_COLOR 1
#endif

#if MINIOS_BOOT_COLOR
#define ANSI_RESET     "\033[0m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_RED       "\033[31m"
#define ANSI_GREEN     "\033[32m"
#define ANSI_YELLOW    "\033[33m"
#define ANSI_BLUE      "\033[94m"
#define ANSI_CYAN      "\033[36m"
#define ANSI_GRAY      "\033[2;37m"
#define ANSI_BOLD_CYAN "\033[1;36m"
#else
#define ANSI_RESET     ""
#define ANSI_BOLD      ""
#define ANSI_RED       ""
#define ANSI_GREEN     ""
#define ANSI_YELLOW    ""
#define ANSI_BLUE      ""
#define ANSI_CYAN      ""
#define ANSI_GRAY      ""
#define ANSI_BOLD_CYAN ""
#endif

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
void term_style_begin(const char *style);
void term_style_end(void);
void term_write_styled(const char *style, const char *text);

#endif
