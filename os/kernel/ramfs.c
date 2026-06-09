#include "ramfs.h"
#include "printk.h"
#include <stddef.h>

/* 固定大小文件表 */
static struct ramfs_file files[RAMFS_MAX_FILES];

/* 将 fd 映射到文件表索引（fd 从 2 开始） */
#define FD_TO_IDX(fd) ((fd) - 2)

void ramfs_init(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        files[i].used = 0;
        files[i].size = 0;
        files[i].name[0] = '\0';
    }
    printk("RAMFS initialized (%d files, %d bytes each)\n",
           RAMFS_MAX_FILES, RAMFS_DATA_SIZE);
}

int ramfs_create(const char *name) {
    if (name == NULL)
        return -1;

    /* 找空闲槽位 */
    int idx = -1;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return -1;

    /* 复制文件名 */
    int i;
    for (i = 0; i < RAMFS_NAME_LEN - 1 && name[i] != '\0'; i++) {
        files[idx].name[i] = name[i];
    }
    files[idx].name[i] = '\0';
    files[idx].size = 0;
    files[idx].used = 1;

    printk("ramfs_create: '%s' -> fd=%d (idx=%d)\n",
           files[idx].name, idx + 2, idx);
    return idx + 2;
}

int ramfs_write(int fd, const void *buf, uint64_t len) {
    int idx = FD_TO_IDX(fd);
    if (idx < 0 || idx >= RAMFS_MAX_FILES || !files[idx].used)
        return -1;

    if (buf == NULL || len == 0)
        return 0;

    /* 截断到文件最大容量 */
    if (len > RAMFS_DATA_SIZE)
        len = RAMFS_DATA_SIZE;

    const char *src = (const char *)buf;
    for (uint64_t i = 0; i < len; i++)
        files[idx].data[i] = src[i];
    files[idx].size = len;

    return (int)len;
}

int ramfs_read(int fd, void *buf, uint64_t len) {
    int idx = FD_TO_IDX(fd);
    if (idx < 0 || idx >= RAMFS_MAX_FILES || !files[idx].used)
        return -1;

    if (buf == NULL || len == 0)
        return 0;

    /* 限制读取长度不超过文件实际大小 */
    if (len > files[idx].size)
        len = files[idx].size;

    char *dst = (char *)buf;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = files[idx].data[i];

    return (int)len;
}

int ramfs_close(int fd) {
    int idx = FD_TO_IDX(fd);
    if (idx < 0 || idx >= RAMFS_MAX_FILES || !files[idx].used)
        return -1;

    files[idx].used = 0;
    files[idx].size = 0;
    files[idx].name[0] = '\0';

    printk("ramfs_close: fd=%d closed\n", fd);
    return 0;
}
