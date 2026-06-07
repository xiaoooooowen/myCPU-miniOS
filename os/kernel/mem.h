#ifndef MINIOS_MEM_H
#define MINIOS_MEM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

/*
 * 物理页分配器
 *
 * 使用空闲链表管理 _kernel_end 到 _stack_top 之间的物理页。
 * 每页 4096 字节，页对齐。
 */

void  mem_init(void);
void *kalloc(void);
void  kfree(void *ptr);
size_t mem_free_pages(void);

#endif
