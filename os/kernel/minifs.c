#include "minifs.h"

#include "block.h"
#include "printk.h"
#include <stddef.h>

#define MINIFS_MAGIC 0x4d465331U
#define MINIFS_VERSION 1U
#define MINIFS_MAX_INODES 64
#define MINIFS_DIRECT_BLOCKS 8
#define MINIFS_MAX_FDS 32
#define MINIFS_INODE_BITMAP_BLOCK 1
#define MINIFS_DATA_BITMAP_BLOCK 2
#define MINIFS_INODE_TABLE_BLOCK 3
#define MINIFS_DATA_START 11

#define MINIFS_FILE 1
#define MINIFS_DIR  2
#define MINIFS_EXEC 3

struct minifs_super {
    uint32_t magic;
    uint32_t version;
    uint32_t blocks;
    uint32_t inode_count;
    uint8_t reserved[BLOCK_SECTOR_SIZE - 16];
};

struct minifs_inode {
    uint32_t type;
    uint32_t size;
    uint32_t parent;
    uint32_t image_id;
    uint32_t direct[MINIFS_DIRECT_BLOCKS];
    uint32_t reserved[4];
};

struct minifs_dirent {
    uint32_t inode;
    uint8_t type;
    uint8_t used;
    uint16_t reserved;
    char name[56];
};

struct minifs_fd {
    int used;
    int owner;
    uint32_t inode;
    uint32_t offset;
};

static struct minifs_fd fd_table[MINIFS_MAX_FDS];
static uint8_t block_buffer[BLOCK_SECTOR_SIZE] __attribute__((aligned(8)));

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

static void zero_block(void) {
    uint64_t *words = (uint64_t *)block_buffer;
    for (int i = 0; i < BLOCK_SECTOR_SIZE / 8; i++)
        words[i] = 0;
}

static void copy_64_bytes(void *destination, const void *source) {
    uint64_t *dst = (uint64_t *)destination;
    const uint64_t *src = (const uint64_t *)source;
    for (int i = 0; i < 8; i++)
        dst[i] = src[i];
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
    uint32_t block = MINIFS_INODE_TABLE_BLOCK + (number >> 3);
    uint32_t offset = (number & 7U) << 6;
    if (block_read(block, block_buffer) < 0)
        return -1;
    copy_64_bytes(inode, block_buffer + offset);
    return 0;
}

static int write_inode(uint32_t number, const struct minifs_inode *inode) {
    if (number >= MINIFS_MAX_INODES || inode == NULL)
        return -1;
    uint32_t block = MINIFS_INODE_TABLE_BLOCK + (number >> 3);
    uint32_t offset = (number & 7U) << 6;
    if (block_read(block, block_buffer) < 0)
        return -1;
    copy_64_bytes(block_buffer + offset, inode);
    return block_write(block, block_buffer);
}

static int alloc_inode(void) {
    if (block_read(MINIFS_INODE_BITMAP_BLOCK, block_buffer) < 0)
        return -1;
    for (uint32_t i = 1; i < MINIFS_MAX_INODES; i++) {
        if (!bitmap_test(block_buffer, i)) {
            bitmap_set(block_buffer, i, 1);
            if (block_write(MINIFS_INODE_BITMAP_BLOCK, block_buffer) < 0)
                return -1;
            return (int)i;
        }
    }
    return -1;
}

static int alloc_data_block(void) {
    if (block_read(MINIFS_DATA_BITMAP_BLOCK, block_buffer) < 0)
        return -1;
    for (uint32_t i = MINIFS_DATA_START; i < BLOCK_SECTOR_COUNT; i++) {
        if (!bitmap_test(block_buffer, i)) {
            bitmap_set(block_buffer, i, 1);
            if (block_write(MINIFS_DATA_BITMAP_BLOCK, block_buffer) < 0)
                return -1;
            zero_block();
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
    if (block_read(MINIFS_DATA_BITMAP_BLOCK, block_buffer) < 0)
        return;
    bitmap_set(block_buffer, block, 0);
    block_write(MINIFS_DATA_BITMAP_BLOCK, block_buffer);
}

static int dir_lookup(uint32_t directory, const char *name,
                      struct minifs_dirent *result) {
    struct minifs_inode inode;
    if (read_inode(directory, &inode) < 0 || inode.type != MINIFS_DIR)
        return -1;

    for (int b = 0; b < MINIFS_DIRECT_BLOCKS; b++) {
        if (inode.direct[b] == 0)
            continue;
        if (block_read(inode.direct[b], block_buffer) < 0)
            return -1;
        struct minifs_dirent *entries = (struct minifs_dirent *)block_buffer;
        for (int i = 0; i < 8; i++) {
            if (entries[i].used && str_equal(entries[i].name, name)) {
                if (result != NULL)
                    copy_64_bytes(result, &entries[i]);
                return (int)entries[i].inode;
            }
        }
    }
    return -1;
}

static int dir_add(uint32_t directory, const char *name, uint32_t child,
                   uint8_t type) {
    struct minifs_inode inode;
    if (read_inode(directory, &inode) < 0 || inode.type != MINIFS_DIR)
        return -1;

    int name_len = str_len(name);
    if (name_len <= 0 || name_len > MINIFS_NAME_MAX)
        return -1;

    for (int b = 0; b < MINIFS_DIRECT_BLOCKS; b++) {
        if (inode.direct[b] == 0) {
            int block = alloc_data_block();
            if (block < 0)
                return -1;
            inode.direct[b] = (uint32_t)block;
            if (write_inode(directory, &inode) < 0)
                return -1;
        }
        if (block_read(inode.direct[b], block_buffer) < 0)
            return -1;
        struct minifs_dirent *entries = (struct minifs_dirent *)block_buffer;
        for (int i = 0; i < 8; i++) {
            if (!entries[i].used) {
                entries[i].used = 1;
                entries[i].inode = child;
                entries[i].type = type;
                for (int j = 0; j <= name_len; j++)
                    entries[i].name[j] = name[j];
                inode.size++;
                if (block_write(inode.direct[b], block_buffer) < 0)
                    return -1;
                return write_inode(directory, &inode);
            }
        }
    }
    return -1;
}

static int next_component(const char **path, char *component) {
    const char *p = *path;
    while (*p == '/')
        p++;
    if (*p == '\0') {
        *path = p;
        component[0] = '\0';
        return 0;
    }

    int len = 0;
    while (*p != '\0' && *p != '/') {
        if (len >= MINIFS_NAME_MAX)
            return -1;
        component[len++] = *p++;
    }
    component[len] = '\0';
    *path = p;
    return 1;
}

static int resolve(uint32_t cwd, const char *path) {
    if (path == NULL || str_len(path) >= MINIFS_PATH_MAX)
        return -1;
    uint32_t current = path[0] == '/' ? MINIFS_ROOT_INODE : cwd;
    char component[MINIFS_NAME_MAX + 1];
    const char *cursor = path;

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

    char parent[MINIFS_PATH_MAX];
    int slash = -1;
    for (int i = 0; i < length; i++)
        if (path[i] == '/')
            slash = i;

    int name_start = slash + 1;
    int name_len = length - name_start;
    if (name_len <= 0 || name_len > MINIFS_NAME_MAX)
        return -1;
    for (int i = 0; i < name_len; i++)
        name[i] = path[name_start + i];
    name[name_len] = '\0';

    if (slash < 0)
        return (int)cwd;
    if (slash == 0)
        return MINIFS_ROOT_INODE;
    for (int i = 0; i < slash; i++)
        parent[i] = path[i];
    parent[slash] = '\0';
    return resolve(cwd, parent);
}

static int create_node(uint32_t parent, const char *name, uint32_t type,
                       uint32_t image_id) {
    int number = alloc_inode();
    if (number < 0)
        return -1;

    struct minifs_inode inode = {0};
    inode.type = type;
    inode.parent = parent;
    inode.image_id = image_id;
    if (write_inode((uint32_t)number, &inode) < 0)
        return -1;
    if (dir_add(parent, name, (uint32_t)number, (uint8_t)type) < 0)
        return -1;
    return number;
}

static int write_inode_data(uint32_t number, const void *data, uint32_t length) {
    struct minifs_inode inode;
    if (read_inode(number, &inode) < 0 || inode.type != MINIFS_FILE)
        return -1;
    inode.size = 0;
    const uint8_t *src = (const uint8_t *)data;
    uint32_t offset = 0;
    while (offset < length && offset < 4096) {
        uint32_t slot = offset >> 9;
        if (inode.direct[slot] == 0) {
            int block = alloc_data_block();
            if (block < 0)
                return -1;
            inode.direct[slot] = (uint32_t)block;
        }
        zero_block();
        uint32_t chunk = length - offset;
        if (chunk > BLOCK_SECTOR_SIZE)
            chunk = BLOCK_SECTOR_SIZE;
        for (uint32_t i = 0; i < chunk; i++)
            block_buffer[i] = src[offset + i];
        if (block_write(inode.direct[slot], block_buffer) < 0)
            return -1;
        offset += chunk;
    }
    inode.size = offset;
    return write_inode(number, &inode);
}

static int format_filesystem(void) {
    zero_block();
    struct minifs_super *super = (struct minifs_super *)block_buffer;
    super->magic = MINIFS_MAGIC;
    super->version = MINIFS_VERSION;
    super->blocks = BLOCK_SECTOR_COUNT;
    super->inode_count = MINIFS_MAX_INODES;
    if (block_write(0, block_buffer) < 0)
        return -1;

    zero_block();
    bitmap_set(block_buffer, MINIFS_ROOT_INODE, 1);
    if (block_write(MINIFS_INODE_BITMAP_BLOCK, block_buffer) < 0)
        return -1;

    zero_block();
    for (uint32_t i = 0; i < MINIFS_DATA_START; i++)
        bitmap_set(block_buffer, i, 1);
    if (block_write(MINIFS_DATA_BITMAP_BLOCK, block_buffer) < 0)
        return -1;

    zero_block();
    for (uint32_t i = MINIFS_INODE_TABLE_BLOCK; i < MINIFS_DATA_START; i++)
        if (block_write(i, block_buffer) < 0)
            return -1;

    struct minifs_inode root = {0};
    root.type = MINIFS_DIR;
    root.parent = MINIFS_ROOT_INODE;
    if (write_inode(MINIFS_ROOT_INODE, &root) < 0)
        return -1;

    int bin = create_node(MINIFS_ROOT_INODE, "bin", MINIFS_DIR, 0);
    int tests = create_node(MINIFS_ROOT_INODE, "tests", MINIFS_DIR, 0);
    int tmp = create_node(MINIFS_ROOT_INODE, "tmp", MINIFS_DIR, 0);
    int readme = create_node(MINIFS_ROOT_INODE, "README", MINIFS_FILE, 0);
    if (bin < 0 || tests < 0 || tmp < 0 || readme < 0)
        return -1;

    if (create_node((uint32_t)tests, "spin", MINIFS_EXEC, 1) < 0 ||
        create_node((uint32_t)tests, "fstest", MINIFS_EXEC, 2) < 0 ||
        create_node((uint32_t)tests, "forktest", MINIFS_EXEC, 3) < 0)
        return -1;

    static const char readme_text[] =
        "MiniOS persistent MiniFS\n"
        "Try: ls /tests, run fstest, run spin &, ps, kill PID\n";
    return write_inode_data((uint32_t)readme, readme_text,
                            sizeof(readme_text) - 1);
}

int minifs_init(void) {
    for (int i = 0; i < MINIFS_MAX_FDS; i++)
        fd_table[i].used = 0;
    if (block_read(0, block_buffer) < 0)
        return -1;
    struct minifs_super *super = (struct minifs_super *)block_buffer;
    if (super->magic == MINIFS_MAGIC) {
        if (super->version == MINIFS_VERSION &&
            super->blocks == BLOCK_SECTOR_COUNT &&
            super->inode_count == MINIFS_MAX_INODES)
            return 0;
        return -1;
    }

    int all_zero = 1;
    for (int i = 0; i < BLOCK_SECTOR_SIZE; i++)
        if (block_buffer[i] != 0)
            all_zero = 0;
    if (!all_zero)
        return -1;
    return format_filesystem();
}

int minifs_open(int owner_pid, uint32_t cwd, const char *path, int flags) {
    int number = resolve(cwd, path);
    if (number < 0 && (flags & MINIFS_O_CREATE)) {
        char name[MINIFS_NAME_MAX + 1];
        int parent = resolve_parent(cwd, path, name);
        if (parent < 0)
            return -1;
        number = create_node((uint32_t)parent, name, MINIFS_FILE, 0);
    }
    if (number < 0)
        return -1;

    struct minifs_inode inode;
    if (read_inode((uint32_t)number, &inode) < 0 ||
        inode.type != MINIFS_FILE)
        return -1;

    if (flags & MINIFS_O_TRUNC) {
        for (int i = 0; i < MINIFS_DIRECT_BLOCKS; i++) {
            free_data_block(inode.direct[i]);
            inode.direct[i] = 0;
        }
        inode.size = 0;
        if (write_inode((uint32_t)number, &inode) < 0)
            return -1;
    }

    for (int i = 0; i < MINIFS_MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].owner = owner_pid;
            fd_table[i].inode = (uint32_t)number;
            fd_table[i].offset = 0;
            return i + 2;
        }
    }
    return -1;
}

static struct minifs_fd *get_fd(int owner_pid, int fd) {
    int index = fd - 2;
    if (index < 0 || index >= MINIFS_MAX_FDS ||
        !fd_table[index].used || fd_table[index].owner != owner_pid)
        return NULL;
    return &fd_table[index];
}

int minifs_close(int owner_pid, int fd) {
    struct minifs_fd *entry = get_fd(owner_pid, fd);
    if (entry == NULL)
        return -1;
    entry->used = 0;
    return 0;
}

int minifs_read(int owner_pid, int fd, void *buffer, uint64_t length) {
    struct minifs_fd *entry = get_fd(owner_pid, fd);
    if (entry == NULL || buffer == NULL)
        return -1;
    struct minifs_inode inode;
    if (read_inode(entry->inode, &inode) < 0)
        return -1;
    if (entry->offset >= inode.size)
        return 0;
    if (length > inode.size - entry->offset)
        length = inode.size - entry->offset;

    uint8_t *dst = (uint8_t *)buffer;
    uint32_t done = 0;
    while (done < length) {
        uint32_t offset = entry->offset;
        uint32_t slot = offset >> 9;
        uint32_t within = offset & (BLOCK_SECTOR_SIZE - 1);
        if (slot >= MINIFS_DIRECT_BLOCKS || inode.direct[slot] == 0)
            break;
        if (block_read(inode.direct[slot], block_buffer) < 0)
            return -1;
        uint32_t chunk = (uint32_t)length - done;
        uint32_t available = BLOCK_SECTOR_SIZE - within;
        if (chunk > available)
            chunk = available;
        for (uint32_t i = 0; i < chunk; i++)
            dst[done + i] = block_buffer[within + i];
        entry->offset += chunk;
        done += chunk;
    }
    return (int)done;
}

int minifs_write(int owner_pid, int fd, const void *buffer, uint64_t length) {
    struct minifs_fd *entry = get_fd(owner_pid, fd);
    if (entry == NULL || buffer == NULL)
        return -1;
    if (length > 4096 - entry->offset)
        length = 4096 - entry->offset;

    struct minifs_inode inode;
    if (read_inode(entry->inode, &inode) < 0)
        return -1;
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t done = 0;
    while (done < length) {
        uint32_t offset = entry->offset;
        uint32_t slot = offset >> 9;
        uint32_t within = offset & (BLOCK_SECTOR_SIZE - 1);
        if (inode.direct[slot] == 0) {
            int block = alloc_data_block();
            if (block < 0)
                break;
            inode.direct[slot] = (uint32_t)block;
            zero_block();
        } else if (block_read(inode.direct[slot], block_buffer) < 0) {
            return -1;
        }
        uint32_t chunk = (uint32_t)length - done;
        uint32_t available = BLOCK_SECTOR_SIZE - within;
        if (chunk > available)
            chunk = available;
        for (uint32_t i = 0; i < chunk; i++)
            block_buffer[within + i] = src[done + i];
        if (block_write(inode.direct[slot], block_buffer) < 0)
            return -1;
        entry->offset += chunk;
        done += chunk;
    }
    if (entry->offset > inode.size)
        inode.size = entry->offset;
    if (write_inode(entry->inode, &inode) < 0)
        return -1;
    return (int)done;
}

int minifs_list(uint32_t cwd, const char *path) {
    int number = (path == NULL || path[0] == '\0') ? (int)cwd
                                                    : resolve(cwd, path);
    if (number < 0)
        return -1;
    struct minifs_inode inode;
    if (read_inode((uint32_t)number, &inode) < 0)
        return -1;
    if (inode.type != MINIFS_DIR) {
        printk("%s\n", path);
        return 0;
    }
    for (int b = 0; b < MINIFS_DIRECT_BLOCKS; b++) {
        if (inode.direct[b] == 0)
            continue;
        if (block_read(inode.direct[b], block_buffer) < 0)
            return -1;
        struct minifs_dirent *entries = (struct minifs_dirent *)block_buffer;
        for (int i = 0; i < 8; i++) {
            if (!entries[i].used)
                continue;
            printk("%s", entries[i].name);
            if (entries[i].type == MINIFS_DIR)
                printk("/");
            else if (entries[i].type == MINIFS_EXEC)
                printk("*");
            printk("\n");
        }
    }
    return 0;
}

int minifs_chdir(uint32_t cwd, const char *path, uint32_t *new_cwd) {
    int number = resolve(cwd, path);
    struct minifs_inode inode;
    if (number < 0 || read_inode((uint32_t)number, &inode) < 0 ||
        inode.type != MINIFS_DIR)
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
        char name[MINIFS_NAME_MAX + 1];
        name[0] = '\0';
        for (int b = 0; b < MINIFS_DIRECT_BLOCKS && name[0] == '\0'; b++) {
            if (parent.direct[b] == 0)
                continue;
            if (block_read(parent.direct[b], block_buffer) < 0)
                return -1;
            struct minifs_dirent *entries =
                (struct minifs_dirent *)block_buffer;
            for (int i = 0; i < 8; i++)
                if (entries[i].used && entries[i].inode == current) {
                    int n = str_len(entries[i].name);
                    for (int j = 0; j <= n; j++)
                        name[j] = entries[i].name[j];
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

int minifs_exec_image(uint32_t cwd, const char *path) {
    int number = resolve(cwd, path);
    int has_slash = 0;
    if (path != NULL)
        for (int i = 0; path[i] != '\0'; i++)
            if (path[i] == '/')
                has_slash = 1;

    if (number < 0 && path != NULL && !has_slash) {
        char candidate[MINIFS_PATH_MAX];
        static const char tests[] = "/tests/";
        int i = 0;
        while (tests[i] != '\0') {
            candidate[i] = tests[i];
            i++;
        }
        int j = 0;
        while (path[j] != '\0' && i < MINIFS_PATH_MAX - 1)
            candidate[i++] = path[j++];
        candidate[i] = '\0';
        number = resolve(cwd, candidate);

        if (number < 0) {
            static const char bin[] = "/bin/";
            i = 0;
            while (bin[i] != '\0') {
                candidate[i] = bin[i];
                i++;
            }
            j = 0;
            while (path[j] != '\0' && i < MINIFS_PATH_MAX - 1)
                candidate[i++] = path[j++];
            candidate[i] = '\0';
            number = resolve(cwd, candidate);
        }
    }
    if (number < 0)
        return -1;
    struct minifs_inode inode;
    if (read_inode((uint32_t)number, &inode) < 0 ||
        inode.type != MINIFS_EXEC)
        return -1;
    return (int)inode.image_id;
}

void minifs_close_all(int owner_pid) {
    for (int i = 0; i < MINIFS_MAX_FDS; i++)
        if (fd_table[i].used && fd_table[i].owner == owner_pid)
            fd_table[i].used = 0;
}
