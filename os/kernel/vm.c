#include "vm.h"
#include "mem.h"
#include "../include/csr.h"

/*
 * 内存布局常量
 */
#define DRAM_BASE   0x80000000ULL
#define DRAM_SIZE   (128ULL * 1024 * 1024)  /* 128 MB */
#define CLINT_BASE  0x02000000ULL
#define PLIC_BASE   0x0C000000ULL
#define PLIC_SIZE   0x04000000ULL           /* 64 MB */
#define UART_BASE   0x10000000ULL
#define SATP_SV39   (8ULL << 60)
#define MEGAPAGE    (2ULL * 1024 * 1024)   /* 2 MB */

void vm_init(void) {
    /*
     * 分配 3 个物理页（共 12KB）：
     *   l2:       根页表（Level 2）
     *   l1_dram:  DRAM 区 LV1 页表（vpn2 = 2，128 MB）
     *   l1_mmio:  MMIO 区 LV1 页表（vpn2 = 0，CLINT/PLIC/UART）
     */
    volatile uint64_t *l2      = (volatile uint64_t *)kalloc();
    volatile uint64_t *l1_dram = (volatile uint64_t *)kalloc();
    volatile uint64_t *l1_mmio = (volatile uint64_t *)kalloc();

    if (!l2 || !l1_dram || !l1_mmio)
        return;

    /* 清零所有页表 */
    for (int i = 0; i < 512; i++) {
        l2[i] = 0;
        l1_dram[i] = 0;
        l1_mmio[i] = 0;
    }

    /*
     * 根页表：vpn2=2 → l1_dram，vpn2=0 → l1_mmio
     */
    l2[0] = PTE((uint64_t)l1_mmio >> 12, PTE_V);
    l2[2] = PTE((uint64_t)l1_dram >> 12, PTE_V);

    /*
     * DRAM 大页映射（2MB 粒度）：
     *   vpn2 = 2, vpn1 = 0..63
     *   映射 0x80000000 到 0x87FFFFFF（128 MB）
     */
    for (int i = 0; i < 64; i++) {
        uint64_t pa = DRAM_BASE + i * MEGAPAGE;
        l1_dram[i] = PTE(pa >> 12, PTE_V | PTE_R | PTE_W | PTE_X);
    }

    /*
     * CLINT 大页映射（2MB 粒度）：
     *   vpn2 = 0, vpn1 = 16
     *   映射 0x02000000 到 0x021FFFFF
     */
    l1_mmio[16] = PTE(CLINT_BASE >> 12, PTE_V | PTE_R | PTE_W);

    /*
     * PLIC 大页映射（2MB 粒度）：
     *   vpn2 = 0, vpn1 = 96..127
     *   映射 0x0C000000 到 0x0FFFFFFF（64 MB）
     */
    for (int i = 0; i < 32; i++) {
        uint64_t pa = PLIC_BASE + i * MEGAPAGE;
        l1_mmio[96 + i] = PTE(pa >> 12, PTE_V | PTE_R | PTE_W);
    }

    /*
     * UART 大页映射（2MB 粒度）：
     *   vpn2 = 0, vpn1 = 128
     *   映射 0x10000000 到 0x101FFFFF
     */
    l1_mmio[128] = PTE(UART_BASE >> 12, PTE_V | PTE_R | PTE_W);

    /*
     * 设置 SATP 开启 Sv39 分页
     */
    uint64_t satp = SATP_SV39 | ((uint64_t)l2 >> 12);
    csr_write(satp, satp);

    /* 刷新 TLB */
    __asm__ volatile("sfence.vma x0, x0");
}
