#ifndef MINIOS_MINIFS_H
#define MINIOS_MINIFS_H

#include <stdint.h>

#define MINIFS_ROOT_INODE 0
#define MINIFS_PATH_MAX 256
#define MINIFS_NAME_MAX 55
#define MINIFS_MAX_FILE_SIZE (64U * 1024U)
#define MINIFS_MAX_FD 16

#define MINIFS_O_RDONLY 0x000
#define MINIFS_O_WRONLY 0x001
#define MINIFS_O_RDWR   0x002
#define MINIFS_O_CREATE 0x040
#define MINIFS_O_TRUNC  0x200
#define MINIFS_O_APPEND 0x400

#define MINIFS_SEEK_SET 0
#define MINIFS_SEEK_CUR 1
#define MINIFS_SEEK_END 2

#define MINIFS_MODE_FILE 1
#define MINIFS_MODE_DIR  2
#define MINIFS_MODE_EXEC 0x100

struct minifs_dirent_info {
    uint32_t inode;
    uint32_t mode;
    char name[56];
};

int minifs_init(void);
int minifs_process_init(int pid);
int minifs_process_fork(int parent_pid, int child_pid);
void minifs_close_all(int pid);

int minifs_open(int pid, uint32_t cwd, const char *path, int flags);
int minifs_close(int pid, int fd);
int minifs_read(int pid, int fd, void *buffer, uint64_t length);
int minifs_write(int pid, int fd, const void *buffer, uint64_t length);
int minifs_lseek(int pid, int fd, int64_t offset, int whence);
int minifs_dup2(int pid, int oldfd, int newfd);

int minifs_getdents(uint32_t cwd, const char *path,
                    struct minifs_dirent_info *entries, int capacity);
int minifs_mkdir(uint32_t cwd, const char *path);
int minifs_unlink(uint32_t cwd, const char *path);
int minifs_chdir(uint32_t cwd, const char *path, uint32_t *new_cwd);
int minifs_getcwd(uint32_t cwd, char *buffer, uint64_t length);

int minifs_resolve_file(uint32_t cwd, const char *path, uint32_t *inode);
int minifs_inode_size(uint32_t inode, uint32_t *size);
int minifs_inode_mode(uint32_t inode, uint32_t *mode);
int minifs_pread(uint32_t inode, uint32_t offset, void *buffer,
                 uint32_t length);

#endif
