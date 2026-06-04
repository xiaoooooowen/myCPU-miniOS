#ifndef MINIOS_VM_H
#define MINIOS_VM_H

#include <stdint.h>

#define PAGE_SIZE 4096

/* PTE 标志位 */
#define PTE_V (1ULL << 0)
#define PTE_R (1ULL << 1)
#define PTE_W (1ULL << 2)
#define PTE_X (1ULL << 3)
#define PTE_U (1ULL << 4)
#define PTE_G (1ULL << 5)
#define PTE_A (1ULL << 6)
#define PTE_D (1ULL << 7)

/*
 * 一级页表条目构造宏。
 * ppn_val: 物理页号 (PA >> 12)
 * flags: PTE 标志位组合 (PTE_R | PTE_W | PTE_X 等)
 */
#define PTE(ppn_val, flags) (((uint64_t)(ppn_val) << 10) | (flags))

/* PA2PTE: 将物理地址转为叶节点 PTE */
#define PA2PTE(pa) PTE((pa) >> 12, PTE_R | PTE_W | PTE_X | PTE_V)

/* 初始化内核 Sv39 页表（身份映射 DRAM 全部 128MB） */
void vm_init(void);

#endif
