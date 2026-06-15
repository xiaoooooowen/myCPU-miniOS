#ifndef MINIOS_MINIFS_H
#define MINIOS_MINIFS_H

#include <stdint.h>

#define MINIFS_ROOT_INODE 0
#define MINIFS_PATH_MAX 128
#define MINIFS_NAME_MAX 55

#define MINIFS_O_CREATE 1
#define MINIFS_O_TRUNC  2

int minifs_init(void);
int minifs_open(int owner_pid, uint32_t cwd, const char *path, int flags);
int minifs_close(int owner_pid, int fd);
int minifs_read(int owner_pid, int fd, void *buffer, uint64_t length);
int minifs_write(int owner_pid, int fd, const void *buffer, uint64_t length);
int minifs_list(uint32_t cwd, const char *path);
int minifs_chdir(uint32_t cwd, const char *path, uint32_t *new_cwd);
int minifs_getcwd(uint32_t cwd, char *buffer, uint64_t length);
int minifs_exec_image(uint32_t cwd, const char *path);
void minifs_close_all(int owner_pid);

#endif
