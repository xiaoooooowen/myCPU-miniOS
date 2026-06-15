#include "syscall.h"

#include "minifs.h"
#include "printk.h"
#include "task.h"
#include "user.h"
#include "../include/csr.h"
#include <stddef.h>

#define TEST_FINISH 0x100000
#define IO_CHUNK 512
#define DIRENT_LIMIT 128

#ifndef MINIOS_BOOT_DIAGNOSTICS
#define MINIOS_BOOT_DIAGNOSTICS 0
#endif

static uint8_t io_buffer[IO_CHUNK];
static struct minifs_dirent_info dirent_buffer[DIRENT_LIMIT];
static struct task_info process_buffer[MAX_TASKS];
static char path_buffer[MINIFS_PATH_MAX];
static char exec_arguments[USER_ARG_MAX][128];
static char exec_environment[USER_ENV_MAX][128];
static const char *exec_argv[USER_ARG_MAX + 1];
static const char *exec_envp[USER_ENV_MAX + 1];

static uint64_t fail(void) {
    return (uint64_t)-1;
}

static int copy_in(void *destination, uint64_t source, uint64_t length) {
    if (task_current_address_space() != NULL)
        return copy_from_user(destination, source, length);
    uint8_t *dst = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;
    for (uint64_t i = 0; i < length; ++i)
        dst[i] = src[i];
    return 0;
}

static int copy_out(uint64_t destination, const void *source,
                    uint64_t length) {
    if (task_current_address_space() != NULL)
        return copy_to_user(destination, source, length);
    uint8_t *dst = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;
    for (uint64_t i = 0; i < length; ++i)
        dst[i] = src[i];
    return 0;
}

static int copy_string_in(char *destination, uint64_t source,
                          uint64_t capacity) {
    if (task_current_address_space() != NULL)
        return copy_string_from_user(destination, source, capacity);
    const char *string = (const char *)source;
    for (uint64_t i = 0; i < capacity; ++i) {
        destination[i] = string[i];
        if (string[i] == '\0')
            return (int)i;
    }
    destination[capacity - 1] = '\0';
    return -1;
}

static int copy_path(uint64_t user_path) {
    if (user_path == 0)
        return -1;
    return copy_string_in(path_buffer, user_path, sizeof(path_buffer));
}

static uint64_t sys_write(uint64_t fd, uint64_t user_buffer,
                          uint64_t length) {
    if (length == 0)
        return 0;
    if (user_buffer == 0)
        return fail();
    uint64_t done = 0;
    while (done < length) {
        uint64_t chunk = length - done;
        if (chunk > sizeof(io_buffer))
            chunk = sizeof(io_buffer);
        if (copy_in(io_buffer, user_buffer + done, chunk) < 0)
            return done == 0 ? fail() : done;
        int written = minifs_write(task_current_pid(), (int)fd,
                                   io_buffer, chunk);
        if (written < 0)
            return done == 0 ? fail() : done;
        done += (uint64_t)written;
        if ((uint64_t)written < chunk)
            break;
    }
    return done;
}

static uint64_t sys_read(uint64_t fd, uint64_t user_buffer,
                         uint64_t length) {
    if (length == 0)
        return 0;
    if (user_buffer == 0)
        return fail();
    uint64_t done = 0;
    while (done < length) {
        uint64_t chunk = length - done;
        if (chunk > sizeof(io_buffer))
            chunk = sizeof(io_buffer);
        int received = minifs_read(task_current_pid(), (int)fd,
                                   io_buffer, chunk);
        if (received < 0)
            return done == 0 ? fail() : done;
        if (received == 0)
            break;
        if (copy_out(user_buffer + done, io_buffer,
                     (uint64_t)received) < 0)
            return done == 0 ? fail() : done;
        done += (uint64_t)received;
        if ((uint64_t)received < chunk)
            break;
    }
    return done;
}

static void sys_exit(uint64_t code) {
#if MINIOS_BOOT_DIAGNOSTICS
    printk("\n--- Task Exit (code=%ld) ---\n", (long)code);
#endif
    task_exit((int)code);
    if (task_current_state() != TASK_ZOMBIE) {
        *(volatile uint32_t *)TEST_FINISH = 0x5555;
        while (1)
            ;
    }
}

static uint64_t sys_waitpid(uint64_t pid, uint64_t user_status,
                            uint64_t options) {
    int status = 0;
    int result = task_waitpid((int)pid, &status, (options & 1) != 0);
    if (result > 0 && user_status != 0 &&
        copy_out(user_status, &status, sizeof(status)) < 0)
        return fail();
    return (uint64_t)result;
}

static uint64_t sys_fork(uint64_t *trap_frame) {
    struct task_address_space *parent = task_current_address_space();
    if (parent == NULL)
        return fail();
    struct task_address_space child;
    if (user_space_clone(&child, parent) < 0)
        return fail();
    int pid = task_fork_from_trap(trap_frame, trap_epc_read() + 4, &child);
    if (pid < 0) {
        user_space_destroy(&child);
        return fail();
    }
    return (uint64_t)pid;
}

static int copy_string_vector(uint64_t user_vector, char storage[][128],
                              const char **result, int maximum,
                              int *total_bytes) {
    if (user_vector == 0) {
        result[0] = NULL;
        return 0;
    }
    for (int i = 0; i < maximum; i++) {
        uint64_t pointer;
        if (copy_in(&pointer, user_vector + i * sizeof(uint64_t),
                    sizeof(pointer)) < 0)
            return -1;
        if (pointer == 0) {
            result[i] = NULL;
            return i;
        }
        int length = copy_string_in(storage[i], pointer, 128);
        if (length < 0 || *total_bytes + length + 1 > USER_STRINGS_MAX)
            return -1;
        *total_bytes += length + 1;
        result[i] = storage[i];
    }
    uint64_t terminator;
    if (copy_in(&terminator, user_vector + maximum * sizeof(uint64_t),
                sizeof(terminator)) < 0 || terminator != 0)
        return -1;
    result[maximum] = NULL;
    return maximum;
}

static int sys_execve(uint64_t *trap_frame, uint64_t user_path,
                      uint64_t user_argv, uint64_t user_envp) {
    if (copy_path(user_path) < 0)
        return -1;
    int total_bytes = 0;
    if (copy_string_vector(user_argv, exec_arguments, exec_argv,
                           USER_ARG_MAX, &total_bytes) < 0 ||
        copy_string_vector(user_envp, exec_environment, exec_envp,
                           USER_ENV_MAX, &total_bytes) < 0)
        return -1;
    if (exec_argv[0] == NULL) {
        exec_argv[0] = path_buffer;
        exec_argv[1] = NULL;
    }
    if (user_execve(trap_frame, path_buffer, exec_argv, exec_envp) < 0)
        return -1;
    task_set_current_name(path_buffer);
    return 0;
}

static uint64_t sys_open(uint64_t user_path, uint64_t flags) {
    if (copy_path(user_path) < 0)
        return fail();
    return (uint64_t)minifs_open(task_current_pid(), task_current_cwd(),
                                 path_buffer, (int)flags);
}

static uint64_t sys_getdents(uint64_t user_path, uint64_t user_entries,
                             uint64_t capacity) {
    const char *path = NULL;
    if (user_path != 0) {
        if (copy_path(user_path) < 0)
            return fail();
        path = path_buffer;
    }
    if (capacity > DIRENT_LIMIT)
        capacity = DIRENT_LIMIT;
    int count = minifs_getdents(task_current_cwd(), path, dirent_buffer,
                                (int)capacity);
    if (count < 0)
        return fail();
    uint64_t bytes = (uint64_t)count * sizeof(dirent_buffer[0]);
    if (bytes != 0 && copy_out(user_entries, dirent_buffer, bytes) < 0)
        return fail();
    return (uint64_t)count;
}

static uint64_t sys_chdir(uint64_t user_path) {
    uint32_t cwd;
    if (copy_path(user_path) < 0 ||
        minifs_chdir(task_current_cwd(), path_buffer, &cwd) < 0)
        return fail();
    task_set_current_cwd(cwd);
    return 0;
}

static uint64_t sys_getcwd(uint64_t user_buffer, uint64_t length) {
    if (length == 0 || length > MINIFS_PATH_MAX)
        return fail();
    int result = minifs_getcwd(task_current_cwd(), path_buffer,
                               sizeof(path_buffer));
    if (result < 0 || (uint64_t)result + 1 > length ||
        copy_out(user_buffer, path_buffer, (uint64_t)result + 1) < 0)
        return fail();
    return (uint64_t)result;
}

static uint64_t sys_mkdir(uint64_t user_path) {
    if (copy_path(user_path) < 0)
        return fail();
    return (uint64_t)minifs_mkdir(task_current_cwd(), path_buffer);
}

static uint64_t sys_unlink(uint64_t user_path) {
    if (copy_path(user_path) < 0)
        return fail();
    return (uint64_t)minifs_unlink(task_current_cwd(), path_buffer);
}

static uint64_t sys_ps(uint64_t user_entries, uint64_t capacity) {
    if (capacity > MAX_TASKS)
        capacity = MAX_TASKS;
    int count = task_get_processes(process_buffer, (int)capacity);
    if (count < 0 ||
        copy_out(user_entries, process_buffer,
                 (uint64_t)count * sizeof(process_buffer[0])) < 0)
        return fail();
    return (uint64_t)count;
}

int syscall_dispatch(uint64_t *trap_frame) {
    uint64_t number = trap_frame[17];
    uint64_t arg0 = trap_frame[10];
    uint64_t arg1 = trap_frame[11];
    uint64_t arg2 = trap_frame[12];
    switch (number) {
        case SYS_READ:
            trap_frame[10] = sys_read(arg0, arg1, arg2);
            break;
        case SYS_WRITE:
            trap_frame[10] = sys_write(arg0, arg1, arg2);
            break;
        case SYS_EXIT:
            sys_exit(arg0);
            break;
        case SYS_WAIT:
            trap_frame[10] = sys_waitpid((uint64_t)-1, 0, 0);
            break;
        case SYS_WAITPID:
            trap_frame[10] = sys_waitpid(arg0, arg1, arg2);
            break;
        case SYS_FORK:
            trap_frame[10] = sys_fork(trap_frame);
            break;
        case SYS_EXECVE:
            if (sys_execve(trap_frame, arg0, arg1, arg2) == 0)
                return 1;
            trap_frame[10] = fail();
            break;
        case SYS_YIELD:
            return 2;
        case SYS_OPEN:
            trap_frame[10] = sys_open(arg0, arg1);
            break;
        case SYS_CLOSE:
            trap_frame[10] = (uint64_t)minifs_close(
                task_current_pid(), (int)arg0);
            break;
        case SYS_LSEEK:
            trap_frame[10] = (uint64_t)minifs_lseek(
                task_current_pid(), (int)arg0, (int64_t)arg1, (int)arg2);
            break;
        case SYS_DUP2:
            trap_frame[10] = (uint64_t)minifs_dup2(
                task_current_pid(), (int)arg0, (int)arg1);
            break;
        case SYS_GETDENTS:
            trap_frame[10] = sys_getdents(arg0, arg1, arg2);
            break;
        case SYS_MKDIR:
            trap_frame[10] = sys_mkdir(arg0);
            break;
        case SYS_UNLINK:
            trap_frame[10] = sys_unlink(arg0);
            break;
        case SYS_CHDIR:
            trap_frame[10] = sys_chdir(arg0);
            break;
        case SYS_GETCWD:
            trap_frame[10] = sys_getcwd(arg0, arg1);
            break;
        case SYS_PS:
            trap_frame[10] = sys_ps(arg0, arg1);
            break;
        case SYS_KILL: {
            int signal = arg1 == 0 ? 15 : (int)arg1;
            if (signal != 9 && signal != 15)
                trap_frame[10] = fail();
            else
                trap_frame[10] = (uint64_t)task_kill(
                    (int)arg0, 128 + signal);
            break;
        }
        default:
            trap_frame[10] = fail();
            break;
    }
    return 0;
}
