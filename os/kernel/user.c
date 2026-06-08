#include "user.h"
#include "mem.h"
#include "vm.h"
#include "printk.h"
#include "../include/csr.h"

/* 用户虚拟地址布局 */
#define USER_TEXT_VA   0x10000ULL
#define USER_STACK_VA  0x20000ULL
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)

/* TEST_FINISH 物理地址 */
#define TEST_FINISH_PA 0x100000ULL

/*
 * 链接器脚本导出的符号：标记 .user_text 段的起止
 */
extern char _user_text_start[];
extern char _user_text_end[];

/* 用户代码页和栈页的物理地址（kalloc 分配） */
static void *user_text_page = NULL;
static void *user_stack_page = NULL;

/* LV0 页表（四级页表最底层，管理 vpn1=0 的 2MB 范围） */
static volatile uint64_t *l0_user = NULL;

void user_init(void) {
    printk("user_init: start\n");
    /* 1. 分配物理页 */
    user_text_page = kalloc();
    printk("user_init: text_page=%lx\n", (uint64_t)user_text_page);
    user_stack_page = kalloc();
    printk("user_init: stack_page=%lx\n", (uint64_t)user_stack_page);
    l0_user = (volatile uint64_t *)kalloc();
    printk("user_init: l0_user=%lx\n", (uint64_t)l0_user);

    if (!user_text_page || !user_stack_page || !l0_user) {
        printk("user_init: kalloc failed!\n");
        return;
    }

    /* 2. 将用户程序 .user_text 段复制到用户代码页 */
    size_t code_size = (size_t)(_user_text_end - _user_text_start);
    printk("user_init: code_size=%d\n", (int)code_size);
    if (code_size > PAGE_SIZE) {
        printk("user_init: user program too large (%d bytes)!\n",
               (int)code_size);
        return;
    }

    char *src = (char *)_user_text_start;
    char *dst = (char *)user_text_page;
    for (size_t i = 0; i < code_size; i++)
        dst[i] = src[i];
    printk("user_init: copied\n");

    /* 3. 初始化 LV0 页表（全部置为无效） */
    printk("user_init: zeroing l0 table...\n");
    for (int i = 0; i < 512; i++)
        l0_user[i] = 0;
    printk("user_init: l0 zeroed\n");

    /*
     * 4. 映射用户代码页：VA 0x10000 → PA user_text_page
     *    vpn2=0, vpn1=0, vpn0=16
     *    权限：U=1, R=1, X=1（用户态可读、可执行）
     */
    l0_user[16] = PTE((uint64_t)user_text_page >> 12,
                       PTE_U | PTE_R | PTE_X | PTE_V);

    /*
     * 5. 映射用户栈页：VA 0x20000 → PA user_stack_page
     *    vpn2=0, vpn1=0, vpn0=32
     *    权限：U=1, R=1, W=1（用户态可读、可写）
     */
    l0_user[32] = PTE((uint64_t)user_stack_page >> 12,
                       PTE_U | PTE_R | PTE_W | PTE_V);

    /*
     * 6. 映射 TEST_FINISH：VA 0x100000 → PA 0x100000
     *    vpn2=0, vpn1=0, vpn0=256
     *    权限：R=1, W=1（内核专用，U=0 禁止用户态访问）
     */
    l0_user[256] = PTE(TEST_FINISH_PA >> 12, PTE_R | PTE_W | PTE_V);

    printk("user_init: ptes set\n");

    /*
     * 7. 更新 l1_mmio[0]：从 2MB 大页切换为指向 l0_user 的非叶 PTE
     *    设置 PTE_U 以允许 MMU 在遍历时进入此子树
     *    l0_user 内部的 PTE 各自控制 U 位（用户页 U=1, TEST_FINISH U=0）
     */
    kernel_l1_mmio[0] = PTE((uint64_t)l0_user >> 12, PTE_V | PTE_U);
    printk("user_init: l1_mmio updated\n");

    /* 8. 刷新 TLB（使新映射生效） */
    __asm__ volatile("sfence.vma x0, x0");

    printk("User mode initialized (text=%lx stack=%lx code=%d bytes)\n",
           (uint64_t)user_text_page, (uint64_t)user_stack_page,
           (int)code_size);
}

/*
 * enter_user() — 通过 sret 从 S 模式切换到 U 模式
 *
 * 设置：
 *   sepc         = 0x10000  (用户程序入口)
 *   sstatus.SPP  = 0        (下一特权级 = User)
 *   sstatus.SPIE = 1        (sret 后开中断)
 *   sp           = 0x21000  (用户栈顶，栈向下增长)
 *
 * sret 执行后：
 *   mode = SPP = User
 *   pc   = sepc = 0x10000
 *   ie   = SPIE = 1
 */
__attribute__((noreturn))
void enter_user(void) {
    uint64_t status = csr_read(sstatus);

    /* 清除 SPP = 0 (User mode) */
    status &= ~SSTATUS_SPP;

    /* 设置 SPIE = 1 (sret 后开启中断) */
    status |= SSTATUS_SPIE;

    csr_write(sstatus, status);

    /* 设置用户入口地址 */
    csr_write(sepc, USER_TEXT_VA);

    /*
     * 设置用户栈指针并执行 sret
     * 注意：此后 CPU 进入 U 模式，内核代码不再执行
     */
    __asm__ volatile(
        "li sp, %0\n"
        "sret\n"
        :
        : "i"(USER_STACK_TOP)
    );

    /* 不会到达此处 */
    __builtin_unreachable();
}
