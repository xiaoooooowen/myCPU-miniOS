#include "mem.h"

/* 链接器脚本导出的符号 */
extern char _kernel_start;
extern char _kernel_end;
extern char _stack_top;

/* 栈保护区域：为内核栈保留 256KB */
#define STACK_GUARD (256 * 1024)

/* 空闲页链表节点，存放在空闲页的起始 8 字节 */
struct free_page {
    struct free_page *next;
};

static struct free_page *free_list = NULL;
static size_t total_free = 0;

/* bump allocator 指针：指向下一块未分配内存的起始地址 */
static uintptr_t bump_ptr = 0;
static uintptr_t heap_end = 0;

/* 向上对齐到页边界 */
static inline uintptr_t align_up(uintptr_t addr, size_t align) {
    return (addr + align - 1) & ~(align - 1);
}

void mem_init(void) {
    bump_ptr = align_up((uintptr_t)&_kernel_end, PAGE_SIZE);
    heap_end = (uintptr_t)&_stack_top - STACK_GUARD;

    /* 按页对齐 */
    heap_end &= ~(PAGE_SIZE - 1);

    free_list = NULL;
    total_free = 0;

    /* 统计可用总页数 */
    if (heap_end > bump_ptr) {
        total_free = (heap_end - bump_ptr) / PAGE_SIZE;
    }
}

void *kalloc(void) {
    /* 优先从空闲链表分配 */
    if (free_list != NULL) {
        struct free_page *page = free_list;
        free_list = page->next;
        total_free--;
        return (void *)page;
    }

    /* 空闲链表为空，使用 bump allocator */
    if (bump_ptr + PAGE_SIZE > heap_end)
        return NULL;

    void *page = (void *)bump_ptr;
    bump_ptr += PAGE_SIZE;
    total_free--;
    return page;
}

void kfree(void *ptr) {
    if (ptr == NULL)
        return;

    uintptr_t addr = (uintptr_t)ptr;

    /* 安全检查：地址必须页对齐 */
    if (addr & (PAGE_SIZE - 1))
        return;

    /* 安全检查：地址必须在合理范围内 */
    if (addr < (uintptr_t)&_kernel_start || addr >= (uintptr_t)&_stack_top)
        return;

    struct free_page *fp = (struct free_page *)addr;
    fp->next = free_list;
    free_list = fp;
    total_free++;
}

size_t mem_free_pages(void) {
    return total_free;
}
