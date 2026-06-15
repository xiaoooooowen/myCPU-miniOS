#include "user.h"
#include <stdarg.h>

#define SYS_DUP2 24
#define SYS_MKDIR 34
#define SYS_UNLINK 35
#define SYS_OPEN 56
#define SYS_CLOSE 57
#define SYS_GETDENTS 61
#define SYS_LSEEK 62
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EXIT 93
#define SYS_FORK 220
#define SYS_EXECVE 221
#define SYS_WAITPID 260
#define SYS_PS 400
#define SYS_CHDIR 402
#define SYS_GETCWD 403
#define SYS_KILL 405

static long syscall3(long number, long first, long second, long third) {
    register long a0 __asm__("a0") = first;
    register long a1 __asm__("a1") = second;
    register long a2 __asm__("a2") = third;
    register long a7 __asm__("a7") = number;
    __asm__ volatile("ecall"
                     : "+r"(a0)
                     : "r"(a1), "r"(a2), "r"(a7)
                     : "memory");
    return a0;
}

long read(int fd, void *buffer, size_t length) {
    return syscall3(SYS_READ, fd, (long)buffer, (long)length);
}

long write(int fd, const void *buffer, size_t length) {
    return syscall3(SYS_WRITE, fd, (long)buffer, (long)length);
}

int open(const char *path, int flags) {
    return (int)syscall3(SYS_OPEN, (long)path, flags, 0);
}

int close(int fd) {
    return (int)syscall3(SYS_CLOSE, fd, 0, 0);
}

long lseek(int fd, long offset, int whence) {
    return syscall3(SYS_LSEEK, fd, offset, whence);
}

int dup2(int oldfd, int newfd) {
    return (int)syscall3(SYS_DUP2, oldfd, newfd, 0);
}

int mkdir(const char *path) {
    return (int)syscall3(SYS_MKDIR, (long)path, 0, 0);
}

int unlink(const char *path) {
    return (int)syscall3(SYS_UNLINK, (long)path, 0, 0);
}

int getdents(const char *path, struct dirent_info *entries, int capacity) {
    return (int)syscall3(SYS_GETDENTS, (long)path, (long)entries, capacity);
}

int chdir(const char *path) {
    return (int)syscall3(SYS_CHDIR, (long)path, 0, 0);
}

int getcwd(char *buffer, int length) {
    return (int)syscall3(SYS_GETCWD, (long)buffer, length, 0);
}

int fork(void) {
    return (int)syscall3(SYS_FORK, 0, 0, 0);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    return (int)syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
}

int waitpid(int pid, int *status, int options) {
    return (int)syscall3(SYS_WAITPID, pid, (long)status, options);
}

int kill(int pid, int signal) {
    return (int)syscall3(SYS_KILL, pid, signal, 0);
}

int getprocs(struct process_info *entries, int capacity) {
    return (int)syscall3(SYS_PS, (long)entries, capacity, 0);
}

void exit(int status) {
    syscall3(SYS_EXIT, status, 0, 0);
    for (;;)
        ;
}

size_t strlen(const char *string) {
    size_t length = 0;
    while (string != 0 && string[length] != '\0')
        length++;
    return length;
}

int strcmp(const char *left, const char *right) {
    while (*left == *right) {
        if (*left == '\0')
            return 0;
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (left[i] != right[i])
            return (unsigned char)left[i] - (unsigned char)right[i];
        if (left[i] == '\0')
            return 0;
    }
    return 0;
}

char *strcpy(char *destination, const char *source) {
    char *result = destination;
    while ((*destination++ = *source++) != '\0')
        ;
    return result;
}

char *strcat(char *destination, const char *source) {
    strcpy(destination + strlen(destination), source);
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    unsigned char *bytes = destination;
    for (size_t i = 0; i < length; i++)
        bytes[i] = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t length) {
    unsigned char *dst = destination;
    const unsigned char *src = source;
    for (size_t i = 0; i < length; i++)
        dst[i] = src[i];
    return destination;
}

int atoi(const char *string) {
    int value = 0;
    int sign = 1;
    if (*string == '-') {
        sign = -1;
        string++;
    }
    while (*string >= '0' && *string <= '9') {
        value = value * 10 + (*string - '0');
        string++;
    }
    return value * sign;
}

char *getenv_from(char *const envp[], const char *name) {
    size_t length = strlen(name);
    if (envp == 0)
        return 0;
    for (int i = 0; envp[i] != 0; i++)
        if (strncmp(envp[i], name, length) == 0 &&
            envp[i][length] == '=')
            return envp[i] + length + 1;
    return 0;
}

int putchar(int character) {
    char byte = (char)character;
    return write(1, &byte, 1) == 1 ? character : -1;
}

int puts(const char *string) {
    if (write(1, string, strlen(string)) < 0 ||
        write(1, "\n", 1) < 0)
        return -1;
    return 0;
}

static void print_unsigned(uint64_t value, unsigned base) {
    char digits[32];
    int count = 0;
    do {
        unsigned digit = (unsigned)(value % base);
        digits[count++] = (char)(digit < 10 ? '0' + digit :
                                'a' + digit - 10);
        value /= base;
    } while (value != 0);
    while (count > 0)
        putchar(digits[--count]);
}

int printf(const char *format, ...) {
    va_list arguments;
    int written = 0;
    va_start(arguments, format);
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            putchar(format[i]);
            written++;
            continue;
        }
        i++;
        int is_long = 0;
        if (format[i] == 'l') {
            is_long = 1;
            i++;
        }
        if (format[i] == 's') {
            const char *string = va_arg(arguments, const char *);
            write(1, string, strlen(string));
        } else if (format[i] == 'c') {
            putchar(va_arg(arguments, int));
        } else if (format[i] == 'd') {
            long value = is_long ? va_arg(arguments, long) :
                                   va_arg(arguments, int);
            if (value < 0) {
                putchar('-');
                value = -value;
            }
            print_unsigned((uint64_t)value, 10);
        } else if (format[i] == 'u') {
            uint64_t value = is_long ? va_arg(arguments, unsigned long) :
                                       va_arg(arguments, unsigned int);
            print_unsigned(value, 10);
        } else if (format[i] == 'x') {
            uint64_t value = is_long ? va_arg(arguments, unsigned long) :
                                       va_arg(arguments, unsigned int);
            print_unsigned(value, 16);
        } else if (format[i] == '%') {
            putchar('%');
        }
    }
    va_end(arguments);
    return written;
}
