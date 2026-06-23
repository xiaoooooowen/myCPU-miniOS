#ifndef MINIOS_RAMFS_H
#define MINIOS_RAMFS_H

#include <stdint.h>

#define RAMFS_MAX_FILES 8
#define RAMFS_NAME_LEN  32
#define RAMFS_DATA_SIZE 4096

/* 内存文件节点 */
struct ramfs_file {
    char     name[RAMFS_NAME_LEN];
    char     data[RAMFS_DATA_SIZE];
    uint64_t size;
    int      used;
};

/* 初始化所有文件槽位 */
void ramfs_init(void);

/*
 * ramfs_create() — 创建一个内存文件
 *
 * 参数：
 *   name  文件名（最多 RAMFS_NAME_LEN-1 字符）
 *
 * 返回值：
 *   成功返回 fd（>= 2），失败返回 -1
 */
int ramfs_create(const char *name);

/*
 * ramfs_write() — 向文件写入数据
 *
 * 参数：
 *   fd   文件描述符
 *   buf  数据缓冲区
 *   len  写入字节数
 *
 * 返回值：实际写入的字节数，失败返回 -1
 */
int ramfs_write(int fd, const void *buf, uint64_t len);

/*
 * ramfs_read() — 从文件读取数据
 *
 * 参数：
 *   fd   文件描述符
 *   buf  数据缓冲区
 *   len  最大读取字节数
 *
 * 返回值：实际读取的字节数，失败返回 -1
 */
int ramfs_read(int fd, void *buf, uint64_t len);

/*
 * ramfs_close() — 关闭文件（释放槽位）
 *
 * 参数：
 *   fd   文件描述符
 *
 * 返回值：成功返回 0，失败返回 -1
 */
int ramfs_close(int fd);

#endif /* MINIOS_RAMFS_H */
