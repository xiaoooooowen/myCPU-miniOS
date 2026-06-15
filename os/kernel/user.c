#include "user.h"
#include "mem.h"
#include "vm.h"
#include "printk.h"
#include "../include/csr.h"

#define TEST_FINISH_PA 0x100000ULL
#define SATP_SV39      (8ULL << 60)

#ifndef MINIOS_BOOT_DIAGNOSTICS
#define MINIOS_BOOT_DIAGNOSTICS 0
#endif

extern char user_entry[];
extern char user_entry_end[];
extern char user_spin[], user_spin_end[];
extern char user_fstest[], user_fstest_end[];
extern char user_forktest[], user_forktest_end[];
extern char user_fork_child[], user_fork_child_end[];

static void zero_page(void *page) {
    uint64_t *words = (uint64_t *)page;
    for (int i = 0; i < PAGE_SIZE / (int)sizeof(uint64_t); i++)
        words[i] = 0;
}

static void copy_page(void *dst, const void *src) {
    uint64_t *dst_words = (uint64_t *)dst;
    const uint64_t *src_words = (const uint64_t *)src;
    for (int i = 0; i < PAGE_SIZE / (int)sizeof(uint64_t); i++)
        dst_words[i] = src_words[i];
}

static int image_bounds(int image_id, const char **start, const char **end) {
    if (image_id == 0) {
        *start = user_entry;
        *end = user_entry_end;
        return 0;
    }
    if (image_id == 1) {
        *start = user_spin;
        *end = user_spin_end;
        return 0;
    }
    if (image_id == 2) {
        *start = user_fstest;
        *end = user_fstest_end;
        return 0;
    }
    if (image_id == 3) {
        *start = user_forktest;
        *end = user_forktest_end;
        return 0;
    }
    if (image_id == 4) {
        *start = user_fork_child;
        *end = user_fork_child_end;
        return 0;
    }
    return -1;
}

static int load_image(void *text_page, int image_id) {
    const char *start;
    const char *end;
    if (image_bounds(image_id, &start, &end) < 0)
        return -1;

    uint64_t size = (uint64_t)(end - start);
    if (size == 0 || size > PAGE_SIZE)
        return -1;

    zero_page(text_page);
    char *dst = (char *)text_page;
    for (uint64_t i = 0; i < size; i++)
        dst[i] = start[i];
    return 0;
}

static void release_partial(struct task_address_space *space) {
    if (space->stack_page != NULL)
        kfree(space->stack_page);
    if (space->text_page != NULL)
        kfree(space->text_page);
    if (space->l0_user != NULL)
        kfree(space->l0_user);
    if (space->l1_mmio != NULL)
        kfree(space->l1_mmio);
    if (space->root != NULL)
        kfree(space->root);
    space->root = NULL;
    space->l1_mmio = NULL;
    space->l0_user = NULL;
    space->text_page = NULL;
    space->stack_page = NULL;
    space->satp = 0;
}

int user_space_create(struct task_address_space *space, int image_id) {
    if (space == NULL || kernel_l2 == NULL || kernel_l1_mmio == NULL)
        return -1;

    space->root = kalloc();
    space->l1_mmio = kalloc();
    space->l0_user = kalloc();
    space->text_page = kalloc();
    space->stack_page = kalloc();
    space->satp = 0;

    if (space->root == NULL || space->l1_mmio == NULL ||
        space->l0_user == NULL || space->text_page == NULL ||
        space->stack_page == NULL) {
        release_partial(space);
        return -1;
    }

    copy_page(space->root, (const void *)kernel_l2);
    copy_page(space->l1_mmio, (const void *)kernel_l1_mmio);
    zero_page(space->l0_user);
    zero_page(space->stack_page);

    if (load_image(space->text_page, image_id) < 0) {
        release_partial(space);
        return -1;
    }

    volatile uint64_t *root = (volatile uint64_t *)space->root;
    volatile uint64_t *l1 = (volatile uint64_t *)space->l1_mmio;
    volatile uint64_t *l0 = (volatile uint64_t *)space->l0_user;

    root[0] = PTE((uint64_t)space->l1_mmio >> 12, PTE_V);
    l1[0] = PTE((uint64_t)space->l0_user >> 12, PTE_V | PTE_U);
    l0[16] = PTE((uint64_t)space->text_page >> 12,
                 PTE_U | PTE_R | PTE_X | PTE_V);
    l0[32] = PTE((uint64_t)space->stack_page >> 12,
                 PTE_U | PTE_R | PTE_W | PTE_V);
    l0[256] = PTE(TEST_FINISH_PA >> 12, PTE_R | PTE_W | PTE_V);

    space->satp = SATP_SV39 | ((uint64_t)space->root >> 12);
    return 0;
}

int user_space_clone(struct task_address_space *dst,
                     const struct task_address_space *src) {
    if (dst == NULL || src == NULL)
        return -1;
    if (user_space_create(dst, 0) < 0)
        return -1;
    copy_page(dst->text_page, src->text_page);
    copy_page(dst->stack_page, src->stack_page);
    return 0;
}

void user_space_destroy(struct task_address_space *space) {
    if (space == NULL)
        return;
    release_partial(space);
}

void user_init(void) {
    struct task_address_space space = {0};
    if (user_space_create(&space, 0) < 0) {
        printk("user_init: address-space allocation failed\n");
        return;
    }
    if (task_attach_address_space(&space) < 0) {
        user_space_destroy(&space);
        printk("user_init: no current process\n");
        return;
    }

    csr_write(satp, space.satp);
    __asm__ volatile("sfence.vma x0, x0");
#if MINIOS_BOOT_DIAGNOSTICS
    printk("User address space ready: pid=%d root=%lx\n",
           task_current_pid(), (uint64_t)space.root);
#endif
}

int user_exec(uint64_t *tf, int image_id) {
    struct task_address_space *space = task_current_address_space();
    if (tf == NULL || space == NULL)
        return -1;
    if (load_image(space->text_page, image_id) < 0)
        return -1;

    zero_page(space->stack_page);
    for (int i = 0; i < TASK_REG_COUNT; i++)
        tf[i] = 0;
    tf[2] = USER_STACK_TOP;
    trap_epc_write(USER_TEXT_VA);
    __asm__ volatile("sfence.vma x0, x0");
    return 0;
}

__attribute__((noreturn))
void enter_user(void) {
    uint64_t status = csr_read(sstatus);
    status &= ~SSTATUS_SPP;
    status |= SSTATUS_SPIE;
    csr_write(sstatus, status);
    csr_write(sepc, USER_TEXT_VA);
    trap_scratch_write(task_current_kernel_stack_top());

    __asm__ volatile(
        "li sp, %0\n"
        "sret\n"
        :
        : "i"(USER_STACK_TOP)
    );
    __builtin_unreachable();
}
