#include "minifs.h"

#include "block.h"
#include "uart.h"
#include <stddef.h>

#define MINIFS_MAGIC 0x4d465332U
#define MINIFS_VERSION 2U
#define MINIFS_MAX_INODES 256U
#define MINIFS_DIRECT_BLOCKS 10
#define MINIFS_INDIRECT_BLOCKS 128
#define MINIFS_MAX_OPEN_FILES 64
#define MINIFS_MAX_PROCESSES 16

#define MINIFS_INODE_BITMAP_START 1U
#define MINIFS_INODE_BITMAP_BLOCKS 1U
#define MINIFS_DATA_BITMAP_START 2U
#define MINIFS_DATA_BITMAP_BLOCKS 4U
#define MINIFS_INODE_TABLE_START 6U
#define MINIFS_INODE_TABLE_BLOCKS 32U
#define MINIFS_DATA_START 38U

#define OFD_CONSOLE_IN  1
#define OFD_CONSOLE_OUT 2
#define OFD_FILE        3

struct minifs_super {
    uint32_t magic;
    uint32_t version;
    uint32_t blocks;
    uint32_t inode_count;
    uint32_t inode_bitmap_start;
    uint32_t inode_bitmap_blocks;
    uint32_t data_bitmap_start;
    uint32_t data_bitmap_blocks;
    uint32_t inode_table_start;
    uint32_t inode_table_blocks;
    uint32_t data_start;
    uint32_t max_file_size;
    uint8_t reserved[BLOCK_SECTOR_SIZE - 48];
};

struct minifs_inode {
    uint32_t mode;
    uint32_t size;
    uint32_t parent;
    uint32_t links;
    uint32_t direct[MINIFS_DIRECT_BLOCKS];
    uint32_t indirect;
    uint32_t reserved;
};

struct minifs_dirent {
    uint32_t inode;
    uint32_t mode;
    char name[56];
};

struct open_file {
    int used;
    int refs;
    int type;
    int flags;
    uint32_t inode;
    uint32_t offset;
};

struct process_fds {
    int used;
    int pid;
    int fd[MINIFS_MAX_FD];
    uint16_t cloexec;
};

static struct open_file open_files[MINIFS_MAX_OPEN_FILES];
static struct process_fds process_fds[MINIFS_MAX_PROCESSES];
static uint8_t block_buffer[BLOCK_SECTOR_SIZE] __attribute__((aligned(8)));
static uint8_t indirect_buffer[BLOCK_SECTOR_SIZE] __attribute__((aligned(8)));
static uint8_t bitmap_buffer[MINIFS_DATA_BITMAP_BLOCKS *
                             BLOCK_SECTOR_SIZE] __attribute__((aligned(8)));

static int str_len(const char *s) {
    int n = 0;
    if (s != NULL)
        while (s[n] != '\0')
            n++;
    return n;
}

static int str_equal(const char *a, const char *b) {
    int i = 0;
    while (a[i] == b[i]) {
        if (a[i] == '\0')
            return 1;
        i++;
    }
    return 0;
}

static void copy_bytes(void *dst_ptr, const void *src_ptr, uint32_t length) {
    uint8_t *dst = (uint8_t *)dst_ptr;
    const uint8_t *src = (const uint8_t *)src_ptr;
    for (uint32_t i = 0; i < length; i++)
        dst[i] = src[i];
}

static void zero_bytes(void *ptr, uint32_t length) {
    uint8_t *bytes = (uint8_t *)ptr;
    for (uint32_t i = 0; i < length; i++)
        bytes[i] = 0;
}

static int bitmap_test(const uint8_t *bitmap, uint32_t bit) {
    return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

static void bitmap_set(uint8_t *bitmap, uint32_t bit, int value) {
    uint8_t mask = (uint8_t)(1U << (bit & 7));
    if (value)
        bitmap[bit >> 3] |= mask;
    else
        bitmap[bit >> 3] &= (uint8_t)~mask;
}

static int read_inode(uint32_t number, struct minifs_inode *inode) {
    if (number >= MINIFS_MAX_INODES || inode == NULL)
        return -1;
    uint32_t block = MINIFS_INODE_TABLE_START + (number >> 3);
    uint32_t offset = (number & 7U) * sizeof(struct minifs_inode);
    if (block_read(block, block_buffer) < 0)
        return -1;
    copy_bytes(inode, block_buffer + offset, sizeof(*inode));
    return 0;
}

static int write_inode(uint32_t number, const struct minifs_inode *inode) {
    if (number >= MINIFS_MAX_INODES || inode == NULL)
        return -1;
    uint32_t block = MINIFS_INODE_TABLE_START + (number >> 3);
    uint32_t offset = (number & 7U) * sizeof(struct minifs_inode);
    if (block_read(block, block_buffer) < 0)
        return -1;
    copy_bytes(block_buffer + offset, inode, sizeof(*inode));
    return block_write(block, block_buffer);
}

static int load_bitmap(uint32_t start, uint32_t count, uint8_t *buffer) {
    for (uint32_t i = 0; i < count; i++)
        if (block_read(start + i, buffer + i * BLOCK_SECTOR_SIZE) < 0)
            return -1;
    return 0;
}

static int alloc_inode(void) {
    if (block_read(MINIFS_INODE_BITMAP_START, block_buffer) < 0)
        return -1;
    for (uint32_t i = 1; i < MINIFS_MAX_INODES; i++) {
        if (!bitmap_test(block_buffer, i)) {
            bitmap_set(block_buffer, i, 1);
            if (block_write(MINIFS_INODE_BITMAP_START, block_buffer) < 0)
                return -1;
            return (int)i;
        }
    }
    return -1;
}

static void free_inode_number(uint32_t inode) {
    if (inode == 0 || inode >= MINIFS_MAX_INODES)
        return;
    if (block_read(MINIFS_INODE_BITMAP_START, block_buffer) < 0)
        return;
    bitmap_set(block_buffer, inode, 0);
    block_write(MINIFS_INODE_BITMAP_START, block_buffer);
}

static int alloc_data_block(void) {
    for (uint32_t i = MINIFS_DATA_START; i < BLOCK_SECTOR_COUNT; i++) {
        if (!bitmap_test(bitmap_buffer, i)) {
            bitmap_set(bitmap_buffer, i, 1);
            uint32_t bitmap_sector = i / (BLOCK_SECTOR_SIZE * 8U);
            if (block_write(MINIFS_DATA_BITMAP_START + bitmap_sector,
                            bitmap_buffer +
                                bitmap_sector * BLOCK_SECTOR_SIZE) < 0)
                return -1;
            zero_bytes(block_buffer, sizeof(block_buffer));
            if (block_write(i, block_buffer) < 0)
                return -1;
            return (int)i;
        }
    }
    return -1;
}

static void free_data_block(uint32_t block) {
    if (block < MINIFS_DATA_START || block >= BLOCK_SECTOR_COUNT)
        return;
    bitmap_set(bitmap_buffer, block, 0);
    uint32_t bitmap_sector = block / (BLOCK_SECTOR_SIZE * 8U);
    block_write(MINIFS_DATA_BITMAP_START + bitmap_sector,
                bitmap_buffer + bitmap_sector * BLOCK_SECTOR_SIZE);
}

static uint32_t inode_block(const struct minifs_inode *inode, uint32_t index) {
    if (index < MINIFS_DIRECT_BLOCKS)
        return inode->direct[index];
    if (index >= MINIFS_DIRECT_BLOCKS + MINIFS_INDIRECT_BLOCKS ||
        inode->indirect == 0)
        return 0;
    if (block_read(inode->indirect, block_buffer) < 0)
        return 0;
    return ((uint32_t *)block_buffer)[index - MINIFS_DIRECT_BLOCKS];
}

static int ensure_inode_block(struct minifs_inode *inode, uint32_t index) {
    if (index >= MINIFS_DIRECT_BLOCKS + MINIFS_INDIRECT_BLOCKS)
        return -1;
    if (index < MINIFS_DIRECT_BLOCKS) {
        if (inode->direct[index] == 0) {
            int block = alloc_data_block();
            if (block < 0)
                return -1;
            inode->direct[index] = (uint32_t)block;
        }
        return (int)inode->direct[index];
    }

    if (inode->indirect == 0) {
        int block = alloc_data_block();
        if (block < 0)
            return -1;
        inode->indirect = (uint32_t)block;
    }
    if (block_read(inode->indirect, indirect_buffer) < 0)
        return -1;
    uint32_t *entries = (uint32_t *)indirect_buffer;
    uint32_t slot = index - MINIFS_DIRECT_BLOCKS;
    if (entries[slot] == 0) {
        int block = alloc_data_block();
        if (block < 0)
            return -1;
        entries[slot] = (uint32_t)block;
        if (block_write(inode->indirect, indirect_buffer) < 0)
            return -1;
    }
    return (int)entries[slot];
}

static void truncate_inode(struct minifs_inode *inode) {
    for (int i = 0; i < MINIFS_DIRECT_BLOCKS; i++) {
        free_data_block(inode->direct[i]);
        inode->direct[i] = 0;
    }
    if (inode->indirect != 0) {
        if (block_read(inode->indirect, indirect_buffer) == 0) {
            uint32_t *entries = (uint32_t *)indirect_buffer;
            for (int i = 0; i < MINIFS_INDIRECT_BLOCKS; i++)
                free_data_block(entries[i]);
        }
        free_data_block(inode->indirect);
        inode->indirect = 0;
    }
    inode->size = 0;
}

static int dir_lookup(uint32_t directory, const char *name,
                      struct minifs_dirent *result) {
    struct minifs_inode inode;
    if (read_inode(directory, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_DIR)
        return -1;
    uint32_t count = inode.size / sizeof(struct minifs_dirent);
    for (uint32_t i = 0; i < count; i++) {
        struct minifs_dirent entry;
        uint32_t block = inode_block(&inode, i / 8);
        if (block == 0 || block_read(block, block_buffer) < 0)
            return -1;
        copy_bytes(&entry, block_buffer + (i % 8) * sizeof(entry),
                   sizeof(entry));
        if (entry.inode != 0xffffffffU && str_equal(entry.name, name)) {
            if (result != NULL)
                *result = entry;
            return (int)entry.inode;
        }
    }
    return -1;
}

static int dir_add(uint32_t directory, const char *name, uint32_t child,
                   uint32_t mode) {
    if (dir_lookup(directory, name, NULL) >= 0)
        return -1;
    int length = str_len(name);
    if (length <= 0 || length > MINIFS_NAME_MAX)
        return -1;
    struct minifs_inode inode;
    if (read_inode(directory, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_DIR)
        return -1;

    uint32_t count = inode.size / sizeof(struct minifs_dirent);
    uint32_t index = count;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t block = inode_block(&inode, i / 8);
        if (block == 0 || block_read(block, block_buffer) < 0)
            return -1;
        struct minifs_dirent *entries = (struct minifs_dirent *)block_buffer;
        if (entries[i % 8].inode == 0xffffffffU) {
            index = i;
            break;
        }
    }
    int block = ensure_inode_block(&inode, index / 8);
    if (block < 0 || block_read((uint32_t)block, block_buffer) < 0)
        return -1;
    struct minifs_dirent *entries = (struct minifs_dirent *)block_buffer;
    struct minifs_dirent *entry = &entries[index % 8];
    zero_bytes(entry, sizeof(*entry));
    entry->inode = child;
    entry->mode = mode;
    for (int i = 0; i <= length; i++)
        entry->name[i] = name[i];
    if (block_write((uint32_t)block, block_buffer) < 0)
        return -1;
    if (index == count)
        inode.size += sizeof(struct minifs_dirent);
    return write_inode(directory, &inode);
}

static int dir_remove(uint32_t directory, const char *name) {
    struct minifs_inode inode;
    if (read_inode(directory, &inode) < 0)
        return -1;
    uint32_t count = inode.size / sizeof(struct minifs_dirent);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t block = inode_block(&inode, i / 8);
        if (block == 0 || block_read(block, block_buffer) < 0)
            return -1;
        struct minifs_dirent *entries = (struct minifs_dirent *)block_buffer;
        if (entries[i % 8].inode != 0xffffffffU &&
            str_equal(entries[i % 8].name, name)) {
            entries[i % 8].inode = 0xffffffffU;
            return block_write(block, block_buffer);
        }
    }
    return -1;
}

static int next_component(const char **path, char *component) {
    const char *p = *path;
    while (*p == '/')
        p++;
    if (*p == '\0') {
        component[0] = '\0';
        *path = p;
        return 0;
    }
    int length = 0;
    while (*p != '\0' && *p != '/') {
        if (length >= MINIFS_NAME_MAX)
            return -1;
        component[length++] = *p++;
    }
    component[length] = '\0';
    *path = p;
    return 1;
}

static int resolve(uint32_t cwd, const char *path) {
    if (path == NULL || path[0] == '\0' ||
        str_len(path) >= MINIFS_PATH_MAX)
        return -1;
    uint32_t current = path[0] == '/' ? MINIFS_ROOT_INODE : cwd;
    const char *cursor = path;
    char component[MINIFS_NAME_MAX + 1];
    while (1) {
        int status = next_component(&cursor, component);
        if (status < 0)
            return -1;
        if (status == 0)
            return (int)current;
        if (str_equal(component, "."))
            continue;
        if (str_equal(component, "..")) {
            struct minifs_inode inode;
            if (read_inode(current, &inode) < 0)
                return -1;
            current = inode.parent;
            continue;
        }
        int child = dir_lookup(current, component, NULL);
        if (child < 0)
            return -1;
        current = (uint32_t)child;
    }
}

static int resolve_parent(uint32_t cwd, const char *path, char *name) {
    int length = str_len(path);
    if (length <= 0 || length >= MINIFS_PATH_MAX)
        return -1;
    while (length > 1 && path[length - 1] == '/')
        length--;
    int slash = -1;
    for (int i = 0; i < length; i++)
        if (path[i] == '/')
            slash = i;
    int start = slash + 1;
    int name_length = length - start;
    if (name_length <= 0 || name_length > MINIFS_NAME_MAX)
        return -1;
    for (int i = 0; i < name_length; i++)
        name[i] = path[start + i];
    name[name_length] = '\0';
    if (str_equal(name, ".") || str_equal(name, ".."))
        return -1;
    if (slash < 0)
        return (int)cwd;
    if (slash == 0)
        return MINIFS_ROOT_INODE;
    char parent[MINIFS_PATH_MAX];
    for (int i = 0; i < slash; i++)
        parent[i] = path[i];
    parent[slash] = '\0';
    return resolve(cwd, parent);
}

static int create_node(uint32_t parent, const char *name, uint32_t mode) {
    int number = alloc_inode();
    if (number < 0)
        return -1;
    struct minifs_inode inode;
    zero_bytes(&inode, sizeof(inode));
    inode.mode = mode;
    inode.parent = parent;
    inode.links = 1;
    if (write_inode((uint32_t)number, &inode) < 0 ||
        dir_add(parent, name, (uint32_t)number, mode) < 0) {
        free_inode_number((uint32_t)number);
        return -1;
    }
    return number;
}

static struct process_fds *find_process(int pid) {
    for (int i = 0; i < MINIFS_MAX_PROCESSES; i++)
        if (process_fds[i].used && process_fds[i].pid == pid)
            return &process_fds[i];
    return NULL;
}

static int alloc_ofd(int type, int flags, uint32_t inode) {
    for (int i = 0; i < MINIFS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].used) {
            open_files[i].used = 1;
            open_files[i].refs = 1;
            open_files[i].type = type;
            open_files[i].flags = flags;
            open_files[i].inode = inode;
            open_files[i].offset = 0;
            return i;
        }
    }
    return -1;
}

static void put_ofd(int index) {
    if (index < 0 || index >= MINIFS_MAX_OPEN_FILES ||
        !open_files[index].used)
        return;
    if (--open_files[index].refs == 0)
        open_files[index].used = 0;
}

static int install_fd(struct process_fds *process, int ofd) {
    for (int fd = 0; fd < MINIFS_MAX_FD; fd++) {
        if (process->fd[fd] < 0) {
            process->fd[fd] = ofd;
            process->cloexec &= (uint16_t)~(1U << fd);
            return fd;
        }
    }
    return -1;
}

static struct open_file *get_open_file(int pid, int fd) {
    struct process_fds *process = find_process(pid);
    if (process == NULL || fd < 0 || fd >= MINIFS_MAX_FD)
        return NULL;
    int index = process->fd[fd];
    if (index < 0 || index >= MINIFS_MAX_OPEN_FILES ||
        !open_files[index].used)
        return NULL;
    return &open_files[index];
}

int minifs_init(void) {
    zero_bytes(open_files, sizeof(open_files));
    zero_bytes(process_fds, sizeof(process_fds));
    if (block_read(0, block_buffer) < 0)
        return -1;
    struct minifs_super *super = (struct minifs_super *)block_buffer;
    if (super->magic != MINIFS_MAGIC ||
        super->version != MINIFS_VERSION ||
        super->blocks != BLOCK_SECTOR_COUNT ||
        super->inode_count != MINIFS_MAX_INODES ||
        super->data_start != MINIFS_DATA_START ||
        super->max_file_size != MINIFS_MAX_FILE_SIZE)
        return -1;
    return load_bitmap(MINIFS_DATA_BITMAP_START,
                       MINIFS_DATA_BITMAP_BLOCKS, bitmap_buffer);
}

int minifs_process_init(int pid) {
    if (find_process(pid) != NULL)
        return 0;
    struct process_fds *process = NULL;
    for (int i = 0; i < MINIFS_MAX_PROCESSES; i++)
        if (!process_fds[i].used) {
            process = &process_fds[i];
            break;
        }
    if (process == NULL)
        return -1;
    process->used = 1;
    process->pid = pid;
    process->cloexec = 0;
    for (int i = 0; i < MINIFS_MAX_FD; i++)
        process->fd[i] = -1;
    int input = alloc_ofd(OFD_CONSOLE_IN, MINIFS_O_RDONLY, 0);
    int output = alloc_ofd(OFD_CONSOLE_OUT, MINIFS_O_WRONLY, 0);
    int error = alloc_ofd(OFD_CONSOLE_OUT, MINIFS_O_WRONLY, 0);
    if (input < 0 || output < 0 || error < 0) {
        put_ofd(input);
        put_ofd(output);
        put_ofd(error);
        process->used = 0;
        return -1;
    }
    process->fd[0] = input;
    process->fd[1] = output;
    process->fd[2] = error;
    return 0;
}

int minifs_process_fork(int parent_pid, int child_pid) {
    struct process_fds *parent = find_process(parent_pid);
    if (parent == NULL || minifs_process_init(child_pid) < 0)
        return -1;
    struct process_fds *child = find_process(child_pid);
    child->cloexec = parent->cloexec;
    for (int fd = 0; fd < MINIFS_MAX_FD; fd++) {
        if (child->fd[fd] >= 0)
            put_ofd(child->fd[fd]);
        child->fd[fd] = parent->fd[fd];
        if (child->fd[fd] >= 0)
            open_files[child->fd[fd]].refs++;
    }
    return 0;
}

void minifs_close_all(int pid) {
    struct process_fds *process = find_process(pid);
    if (process == NULL)
        return;
    for (int fd = 0; fd < MINIFS_MAX_FD; fd++) {
        if (process->fd[fd] >= 0)
            put_ofd(process->fd[fd]);
        process->fd[fd] = -1;
    }
    process->cloexec = 0;
    process->used = 0;
}

int minifs_set_cloexec(int pid, int fd, int on) {
    struct process_fds *process = find_process(pid);
    if (process == NULL || fd < 0 || fd >= MINIFS_MAX_FD ||
        process->fd[fd] < 0)
        return -1;
    if (on)
        process->cloexec |= (uint16_t)(1U << fd);
    else
        process->cloexec &= (uint16_t)~(1U << fd);
    return 0;
}

void minifs_close_exec_fds(int pid) {
    struct process_fds *process = find_process(pid);
    if (process == NULL)
        return;
    for (int fd = 0; fd < MINIFS_MAX_FD; fd++) {
        if (process->cloexec & (uint16_t)(1U << fd)) {
            if (process->fd[fd] >= 0)
                put_ofd(process->fd[fd]);
            process->fd[fd] = -1;
            process->cloexec &= (uint16_t)~(1U << fd);
        }
    }
}

int minifs_open(int pid, uint32_t cwd, const char *path, int flags) {
    struct process_fds *process = find_process(pid);
    int access = flags & 3;
    int known_flags = 3 | MINIFS_O_CREATE |
                      MINIFS_O_TRUNC | MINIFS_O_APPEND;
    if (process == NULL || access == 3 || (flags & ~known_flags) != 0 ||
        (access == MINIFS_O_RDONLY &&
         (flags & (MINIFS_O_TRUNC | MINIFS_O_APPEND))))
        return -1;
    int number = resolve(cwd, path);
    if (number < 0 && (flags & MINIFS_O_CREATE)) {
        char name[MINIFS_NAME_MAX + 1];
        int parent = resolve_parent(cwd, path, name);
        if (parent < 0)
            return -1;
        number = create_node((uint32_t)parent, name, MINIFS_MODE_FILE);
    }
    if (number < 0)
        return -1;
    struct minifs_inode inode;
    if (read_inode((uint32_t)number, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_FILE)
        return -1;
    if (flags & MINIFS_O_TRUNC) {
        truncate_inode(&inode);
        if (write_inode((uint32_t)number, &inode) < 0)
            return -1;
    }
    int ofd = alloc_ofd(OFD_FILE, flags, (uint32_t)number);
    if (ofd < 0)
        return -1;
    if (flags & MINIFS_O_APPEND)
        open_files[ofd].offset = inode.size;
    int fd = install_fd(process, ofd);
    if (fd < 0)
        put_ofd(ofd);
    return fd;
}

int minifs_close(int pid, int fd) {
    struct process_fds *process = find_process(pid);
    if (process == NULL || fd < 0 || fd >= MINIFS_MAX_FD ||
        process->fd[fd] < 0)
        return -1;
    put_ofd(process->fd[fd]);
    process->fd[fd] = -1;
    process->cloexec &= (uint16_t)~(1U << fd);
    return 0;
}

int minifs_pread(uint32_t inode_number, uint32_t offset, void *buffer,
                 uint32_t length) {
    struct minifs_inode inode;
    if (buffer == NULL || read_inode(inode_number, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_FILE)
        return -1;
    if (offset >= inode.size)
        return 0;
    if (length > inode.size - offset)
        length = inode.size - offset;
    uint8_t *destination = (uint8_t *)buffer;
    uint32_t done = 0;
    while (done < length) {
        uint32_t logical = offset / BLOCK_SECTOR_SIZE;
        uint32_t within = offset % BLOCK_SECTOR_SIZE;
        uint32_t block = inode_block(&inode, logical);
        if (block == 0 || block_read(block, block_buffer) < 0)
            return -1;
        uint32_t chunk = length - done;
        if (chunk > BLOCK_SECTOR_SIZE - within)
            chunk = BLOCK_SECTOR_SIZE - within;
        copy_bytes(destination + done, block_buffer + within, chunk);
        offset += chunk;
        done += chunk;
    }
    return (int)done;
}

int minifs_read(int pid, int fd, void *buffer, uint64_t length) {
    struct open_file *file = get_open_file(pid, fd);
    if (file == NULL || buffer == NULL)
        return -1;
    if (file->type == OFD_CONSOLE_IN) {
        uint8_t *bytes = (uint8_t *)buffer;
        uint64_t done = 0;
        while (done < length) {
            bytes[done++] = (uint8_t)uart_getc();
            if (bytes[done - 1] == '\n')
                break;
        }
        return (int)done;
    }
    if (file->type != OFD_FILE ||
        (file->flags & 3) == MINIFS_O_WRONLY)
        return -1;
    int result = minifs_pread(file->inode, file->offset, buffer,
                              length > 0xffffffffU ? 0xffffffffU :
                              (uint32_t)length);
    if (result > 0)
        file->offset += (uint32_t)result;
    return result;
}

static int write_at(uint32_t inode_number, uint32_t offset,
                    const void *buffer, uint32_t length) {
    if (offset >= MINIFS_MAX_FILE_SIZE)
        return 0;
    if (length > MINIFS_MAX_FILE_SIZE - offset)
        length = MINIFS_MAX_FILE_SIZE - offset;
    struct minifs_inode inode;
    if (read_inode(inode_number, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_FILE)
        return -1;
    const uint8_t *source = (const uint8_t *)buffer;
    uint32_t done = 0;
    while (done < length) {
        uint32_t logical = offset / BLOCK_SECTOR_SIZE;
        uint32_t within = offset % BLOCK_SECTOR_SIZE;
        int block = ensure_inode_block(&inode, logical);
        if (block < 0 || block_read((uint32_t)block, block_buffer) < 0)
            break;
        uint32_t chunk = length - done;
        if (chunk > BLOCK_SECTOR_SIZE - within)
            chunk = BLOCK_SECTOR_SIZE - within;
        copy_bytes(block_buffer + within, source + done, chunk);
        if (block_write((uint32_t)block, block_buffer) < 0)
            return -1;
        offset += chunk;
        done += chunk;
    }
    if (offset > inode.size)
        inode.size = offset;
    if (write_inode(inode_number, &inode) < 0)
        return -1;
    return (int)done;
}

int minifs_write(int pid, int fd, const void *buffer, uint64_t length) {
    struct open_file *file = get_open_file(pid, fd);
    if (file == NULL || buffer == NULL)
        return -1;
    if (file->type == OFD_CONSOLE_OUT) {
        const char *bytes = (const char *)buffer;
        for (uint64_t i = 0; i < length; i++)
            uart_putc(bytes[i]);
        return (int)length;
    }
    if (file->type != OFD_FILE ||
        (file->flags & 3) == MINIFS_O_RDONLY)
        return -1;
    if (file->flags & MINIFS_O_APPEND) {
        struct minifs_inode inode;
        if (read_inode(file->inode, &inode) < 0)
            return -1;
        file->offset = inode.size;
    }
    int result = write_at(file->inode, file->offset, buffer,
                          length > 0xffffffffU ? 0xffffffffU :
                          (uint32_t)length);
    if (result > 0)
        file->offset += (uint32_t)result;
    return result;
}

int minifs_lseek(int pid, int fd, int64_t offset, int whence) {
    struct open_file *file = get_open_file(pid, fd);
    if (file == NULL || file->type != OFD_FILE)
        return -1;
    int64_t base = 0;
    if (whence == MINIFS_SEEK_CUR)
        base = file->offset;
    else if (whence == MINIFS_SEEK_END) {
        struct minifs_inode inode;
        if (read_inode(file->inode, &inode) < 0)
            return -1;
        base = inode.size;
    } else if (whence != MINIFS_SEEK_SET) {
        return -1;
    }
    int64_t position = base + offset;
    if (position < 0 || position > MINIFS_MAX_FILE_SIZE)
        return -1;
    file->offset = (uint32_t)position;
    return (int)position;
}

int minifs_dup2(int pid, int oldfd, int newfd) {
    struct process_fds *process = find_process(pid);
    if (process == NULL || oldfd < 0 || oldfd >= MINIFS_MAX_FD ||
        newfd < 0 || newfd >= MINIFS_MAX_FD || process->fd[oldfd] < 0)
        return -1;
    if (oldfd == newfd)
        return newfd;
    if (process->fd[newfd] >= 0)
        put_ofd(process->fd[newfd]);
    process->fd[newfd] = process->fd[oldfd];
    open_files[process->fd[newfd]].refs++;
    process->cloexec &= (uint16_t)~(1U << newfd);
    return newfd;
}

int minifs_getdents(uint32_t cwd, const char *path,
                    struct minifs_dirent_info *entries, int capacity) {
    int number = (path == NULL || path[0] == '\0') ? (int)cwd :
                 resolve(cwd, path);
    if (number < 0 || entries == NULL || capacity < 0)
        return -1;
    struct minifs_inode inode;
    if (read_inode((uint32_t)number, &inode) < 0)
        return -1;
    if ((inode.mode & 0xff) == MINIFS_MODE_FILE) {
        if (capacity == 0)
            return 0;
        entries[0].inode = (uint32_t)number;
        entries[0].mode = inode.mode;
        int length = str_len(path);
        int start = length - 1;
        while (start >= 0 && path[start] != '/')
            start--;
        start++;
        int i = 0;
        while (path[start + i] != '\0' && i < MINIFS_NAME_MAX) {
            entries[0].name[i] = path[start + i];
            i++;
        }
        entries[0].name[i] = '\0';
        return 1;
    }
    int written = 0;
    uint32_t count = inode.size / sizeof(struct minifs_dirent);
    for (uint32_t i = 0; i < count && written < capacity; i++) {
        uint32_t block = inode_block(&inode, i / 8);
        if (block == 0 || block_read(block, block_buffer) < 0)
            return -1;
        struct minifs_dirent *source =
            &((struct minifs_dirent *)block_buffer)[i % 8];
        if (source->inode == 0xffffffffU)
            continue;
        entries[written].inode = source->inode;
        entries[written].mode = source->mode;
        copy_bytes(entries[written].name, source->name, sizeof(source->name));
        written++;
    }
    return written;
}

int minifs_mkdir(uint32_t cwd, const char *path) {
    char name[MINIFS_NAME_MAX + 1];
    int parent = resolve_parent(cwd, path, name);
    if (parent < 0)
        return -1;
    return create_node((uint32_t)parent, name, MINIFS_MODE_DIR) < 0 ? -1 : 0;
}

static int inode_is_open(uint32_t inode) {
    for (int i = 0; i < MINIFS_MAX_OPEN_FILES; i++)
        if (open_files[i].used && open_files[i].type == OFD_FILE &&
            open_files[i].inode == inode)
            return 1;
    return 0;
}

int minifs_unlink(uint32_t cwd, const char *path) {
    char name[MINIFS_NAME_MAX + 1];
    int parent = resolve_parent(cwd, path, name);
    if (parent < 0)
        return -1;
    int number = dir_lookup((uint32_t)parent, name, NULL);
    if (number <= 0 || inode_is_open((uint32_t)number))
        return -1;
    struct minifs_inode inode;
    if (read_inode((uint32_t)number, &inode) < 0)
        return -1;
    if ((inode.mode & 0xff) == MINIFS_MODE_DIR) {
        uint32_t count = inode.size / sizeof(struct minifs_dirent);
        for (uint32_t i = 0; i < count; i++) {
            uint32_t block = inode_block(&inode, i / 8);
            if (block == 0 || block_read(block, block_buffer) < 0)
                return -1;
            if (((struct minifs_dirent *)block_buffer)[i % 8].inode !=
                0xffffffffU)
                return -1;
        }
    }
    truncate_inode(&inode);
    zero_bytes(&inode, sizeof(inode));
    if (write_inode((uint32_t)number, &inode) < 0 ||
        dir_remove((uint32_t)parent, name) < 0)
        return -1;
    free_inode_number((uint32_t)number);
    return 0;
}

int minifs_chdir(uint32_t cwd, const char *path, uint32_t *new_cwd) {
    int number = resolve(cwd, path);
    struct minifs_inode inode;
    if (number < 0 || new_cwd == NULL ||
        read_inode((uint32_t)number, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_DIR)
        return -1;
    *new_cwd = (uint32_t)number;
    return 0;
}

int minifs_getcwd(uint32_t cwd, char *buffer, uint64_t length) {
    if (buffer == NULL || length < 2)
        return -1;
    if (cwd == MINIFS_ROOT_INODE) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return 1;
    }
    char reverse[MINIFS_PATH_MAX];
    int used = 0;
    uint32_t current = cwd;
    while (current != MINIFS_ROOT_INODE) {
        struct minifs_inode inode;
        if (read_inode(current, &inode) < 0)
            return -1;
        struct minifs_inode parent;
        if (read_inode(inode.parent, &parent) < 0)
            return -1;
        uint32_t count = parent.size / sizeof(struct minifs_dirent);
        char name[MINIFS_NAME_MAX + 1];
        name[0] = '\0';
        for (uint32_t i = 0; i < count; i++) {
            uint32_t block = inode_block(&parent, i / 8);
            if (block == 0 || block_read(block, block_buffer) < 0)
                return -1;
            struct minifs_dirent *entry =
                &((struct minifs_dirent *)block_buffer)[i % 8];
            if (entry->inode == current) {
                copy_bytes(name, entry->name, sizeof(entry->name));
                break;
            }
        }
        int n = str_len(name);
        if (n == 0 || used + n + 1 >= MINIFS_PATH_MAX)
            return -1;
        for (int i = n - 1; i >= 0; i--)
            reverse[used++] = name[i];
        reverse[used++] = '/';
        current = inode.parent;
    }
    if ((uint64_t)used + 1 > length)
        return -1;
    for (int i = 0; i < used; i++)
        buffer[i] = reverse[used - 1 - i];
    buffer[used] = '\0';
    return used;
}

int minifs_resolve_file(uint32_t cwd, const char *path, uint32_t *inode_out) {
    int number = resolve(cwd, path);
    struct minifs_inode inode;
    if (number < 0 || inode_out == NULL ||
        read_inode((uint32_t)number, &inode) < 0 ||
        (inode.mode & 0xff) != MINIFS_MODE_FILE)
        return -1;
    *inode_out = (uint32_t)number;
    return 0;
}

int minifs_inode_size(uint32_t inode_number, uint32_t *size) {
    struct minifs_inode inode;
    if (size == NULL || read_inode(inode_number, &inode) < 0)
        return -1;
    *size = inode.size;
    return 0;
}

int minifs_inode_mode(uint32_t inode_number, uint32_t *mode) {
    struct minifs_inode inode;
    if (mode == NULL || read_inode(inode_number, &inode) < 0)
        return -1;
    *mode = inode.mode;
    return 0;
}
