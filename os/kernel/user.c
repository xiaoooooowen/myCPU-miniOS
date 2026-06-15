#include "user.h"

#include "block.h"
#include "mem.h"
#include "minifs.h"
#include "printk.h"
#include "vm.h"
#include "../include/csr.h"

#define SATP_SV39 (8ULL << 60)
#define ELF_PT_LOAD 1U
#define ELF_PF_X 1U
#define ELF_PF_W 2U
#define ELF_PF_R 4U
#define ELF_ET_EXEC 2U
#define ELF_EM_RISCV 243U

#ifndef MINIOS_BOOT_DIAGNOSTICS
#define MINIOS_BOOT_DIAGNOSTICS 0
#endif

struct elf64_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

struct loaded_program {
    struct task_address_space space;
    uint64_t entry;
    uint64_t stack_pointer;
    uint64_t argv_pointer;
    uint64_t envp_pointer;
    int argc;
};

static void zero_page(void *page) {
    uint64_t *words = (uint64_t *)page;
    for (int i = 0; i < PAGE_SIZE / (int)sizeof(uint64_t); i++)
        words[i] = 0;
}

static void copy_page(void *destination, const void *source) {
    uint64_t *dst = (uint64_t *)destination;
    const uint64_t *src = (const uint64_t *)source;
    for (int i = 0; i < PAGE_SIZE / (int)sizeof(uint64_t); i++)
        dst[i] = src[i];
}

static void zero_space(struct task_address_space *space) {
    space->root = NULL;
    space->l1_mmio = NULL;
    space->l0_user = NULL;
    space->page_count = 0;
    space->satp = 0;
    for (int i = 0; i < USER_MAX_PAGES; i++) {
        space->pages[i].va = 0;
        space->pages[i].page = NULL;
        space->pages[i].flags = 0;
    }
}

static int create_empty_space(struct task_address_space *space) {
    if (space == NULL || kernel_l2 == NULL || kernel_l1_mmio == NULL)
        return -1;
    zero_space(space);
    space->root = kalloc();
    space->l1_mmio = kalloc();
    space->l0_user = kalloc();
    if (space->root == NULL || space->l1_mmio == NULL ||
        space->l0_user == NULL) {
        user_space_destroy(space);
        return -1;
    }
    copy_page(space->root, (const void *)kernel_l2);
    copy_page(space->l1_mmio, (const void *)kernel_l1_mmio);
    zero_page(space->l0_user);
    volatile uint64_t *root = (volatile uint64_t *)space->root;
    volatile uint64_t *l1 = (volatile uint64_t *)space->l1_mmio;
    root[0] = PTE((uint64_t)space->l1_mmio >> 12, PTE_V);
    l1[0] = PTE((uint64_t)space->l0_user >> 12, PTE_V | PTE_U);
    space->satp = SATP_SV39 | ((uint64_t)space->root >> 12);
    return 0;
}

static struct user_page_mapping *find_mapping(
    struct task_address_space *space, uint64_t va) {
    uint64_t page_va = va & ~(PAGE_SIZE - 1ULL);
    for (uint32_t i = 0; i < space->page_count; i++)
        if (space->pages[i].va == page_va)
            return &space->pages[i];
    return NULL;
}

static int map_user_page(struct task_address_space *space, uint64_t va,
                         uint64_t flags) {
    if (space == NULL || va < USER_MIN_VA || va >= USER_STACK_TOP ||
        (va & (PAGE_SIZE - 1)) != 0)
        return -1;
    struct user_page_mapping *existing = find_mapping(space, va);
    if (existing != NULL) {
        existing->flags |= flags;
        ((uint64_t *)space->l0_user)[(va >> 12) & 0x1ff] =
            PTE((uint64_t)existing->page >> 12,
                existing->flags | PTE_U | PTE_V);
        return 0;
    }
    if (space->page_count >= USER_MAX_PAGES)
        return -1;
    void *page = kalloc();
    if (page == NULL)
        return -1;
    zero_page(page);
    struct user_page_mapping *mapping = &space->pages[space->page_count++];
    mapping->va = va;
    mapping->page = page;
    mapping->flags = flags;
    ((uint64_t *)space->l0_user)[(va >> 12) & 0x1ff] =
        PTE((uint64_t)page >> 12, flags | PTE_U | PTE_V);
    return 0;
}

static int copy_to_space(struct task_address_space *space, uint64_t destination,
                         const void *source_ptr, uint64_t length) {
    const uint8_t *source = (const uint8_t *)source_ptr;
    uint64_t done = 0;
    while (done < length) {
        struct user_page_mapping *mapping =
            find_mapping(space, destination + done);
        if (mapping == NULL)
            return -1;
        uint64_t within = (destination + done) & (PAGE_SIZE - 1);
        uint64_t chunk = length - done;
        if (chunk > PAGE_SIZE - within)
            chunk = PAGE_SIZE - within;
        uint8_t *target = (uint8_t *)mapping->page + within;
        for (uint64_t i = 0; i < chunk; i++)
            target[i] = source[i + done];
        done += chunk;
    }
    return 0;
}

static int copy_from_space(const struct task_address_space *space,
                           void *destination_ptr, uint64_t source,
                           uint64_t length) {
    uint8_t *destination = (uint8_t *)destination_ptr;
    uint64_t done = 0;
    while (done < length) {
        struct user_page_mapping *mapping = find_mapping(
            (struct task_address_space *)space, source + done);
        if (mapping == NULL)
            return -1;
        uint64_t within = (source + done) & (PAGE_SIZE - 1);
        uint64_t chunk = length - done;
        if (chunk > PAGE_SIZE - within)
            chunk = PAGE_SIZE - within;
        const uint8_t *bytes = (const uint8_t *)mapping->page + within;
        for (uint64_t i = 0; i < chunk; i++)
            destination[i + done] = bytes[i];
        done += chunk;
    }
    return 0;
}

static int valid_elf_header(const struct elf64_header *header,
                            uint32_t file_size) {
    return header->ident[0] == 0x7f &&
           header->ident[1] == 'E' &&
           header->ident[2] == 'L' &&
           header->ident[3] == 'F' &&
           header->ident[4] == 2 &&
           header->ident[5] == 1 &&
           header->ident[6] == 1 &&
           header->type == ELF_ET_EXEC &&
           header->machine == ELF_EM_RISCV &&
           header->version == 1 &&
           header->ehsize == sizeof(struct elf64_header) &&
           header->phentsize == sizeof(struct elf64_program_header) &&
           header->phnum > 0 && header->phnum <= 16 &&
           header->phoff <= file_size &&
           header->phoff +
               (uint64_t)header->phnum * header->phentsize <= file_size &&
           header->entry >= USER_MIN_VA &&
           header->entry < USER_STACK_BOTTOM;
}

static int load_elf(uint32_t inode, struct loaded_program *program) {
    uint32_t file_size;
    uint32_t mode;
    struct elf64_header header;
    if (minifs_inode_size(inode, &file_size) < 0 ||
        minifs_inode_mode(inode, &mode) < 0 ||
        !(mode & MINIFS_MODE_EXEC) ||
        minifs_pread(inode, 0, &header, sizeof(header)) != sizeof(header) ||
        !valid_elf_header(&header, file_size))
        return -1;
    if (create_empty_space(&program->space) < 0)
        return -1;

    for (uint16_t i = 0; i < header.phnum; i++) {
        struct elf64_program_header segment;
        uint32_t offset = (uint32_t)(header.phoff +
                                    (uint64_t)i * header.phentsize);
        if (minifs_pread(inode, offset, &segment, sizeof(segment)) !=
            sizeof(segment))
            goto fail;
        if (segment.type != ELF_PT_LOAD)
            continue;
        if (segment.memsz == 0) {
            if (segment.filesz != 0)
                goto fail;
            continue;
        }
        if (segment.filesz > segment.memsz ||
            segment.offset > file_size ||
            segment.filesz > file_size - segment.offset ||
            segment.vaddr < USER_MIN_VA ||
            segment.vaddr + segment.memsz < segment.vaddr ||
            segment.vaddr + segment.memsz > USER_STACK_BOTTOM)
            goto fail;
        uint64_t flags = 0;
        if (segment.flags & ELF_PF_R)
            flags |= PTE_R;
        if (segment.flags & ELF_PF_W)
            flags |= PTE_W;
        if (segment.flags & ELF_PF_X)
            flags |= PTE_X;
        if ((flags & (PTE_R | PTE_W | PTE_X)) == 0 ||
            ((flags & PTE_W) && !(flags & PTE_R)))
            goto fail;
        uint64_t start = segment.vaddr & ~(PAGE_SIZE - 1ULL);
        uint64_t end = (segment.vaddr + segment.memsz + PAGE_SIZE - 1) &
                       ~(PAGE_SIZE - 1ULL);
        for (uint64_t va = start; va < end; va += PAGE_SIZE)
            if (map_user_page(&program->space, va, flags) < 0)
                goto fail;

        uint8_t scratch[BLOCK_SECTOR_SIZE];
        uint64_t copied = 0;
        while (copied < segment.filesz) {
            uint32_t chunk = (uint32_t)(segment.filesz - copied);
            if (chunk > sizeof(scratch))
                chunk = sizeof(scratch);
            if (minifs_pread(inode, (uint32_t)(segment.offset + copied),
                             scratch, chunk) != (int)chunk ||
                copy_to_space(&program->space, segment.vaddr + copied,
                              scratch, chunk) < 0)
                goto fail;
            copied += chunk;
        }
    }
    if (find_mapping(&program->space, header.entry) == NULL ||
        !(find_mapping(&program->space, header.entry)->flags & PTE_X))
        goto fail;
    for (uint64_t va = USER_STACK_BOTTOM; va < USER_STACK_TOP;
         va += PAGE_SIZE)
        if (map_user_page(&program->space, va, PTE_R | PTE_W) < 0)
            goto fail;
    program->entry = header.entry;
    return 0;
fail:
    user_space_destroy(&program->space);
    return -1;
}

static int string_length_limited(const char *string, int limit) {
    int length = 0;
    if (string == NULL)
        return -1;
    while (length < limit && string[length] != '\0')
        length++;
    return length == limit ? -1 : length;
}

static int build_initial_stack(struct loaded_program *program,
                               const char *const argv[],
                               const char *const envp[]) {
    uint64_t stack = USER_STACK_TOP;
    uint64_t argv_addresses[USER_ARG_MAX];
    uint64_t env_addresses[USER_ENV_MAX];
    int argc = 0;
    int envc = 0;
    int string_bytes = 0;
    while (argv != NULL && argv[argc] != NULL && argc < USER_ARG_MAX) {
        int length = string_length_limited(argv[argc], USER_STRINGS_MAX);
        if (length < 0 || string_bytes + length + 1 > USER_STRINGS_MAX)
            return -1;
        string_bytes += length + 1;
        argc++;
    }
    if (argv != NULL && argc == USER_ARG_MAX && argv[argc] != NULL)
        return -1;
    while (envp != NULL && envp[envc] != NULL && envc < USER_ENV_MAX) {
        int length = string_length_limited(envp[envc], USER_STRINGS_MAX);
        if (length < 0 || string_bytes + length + 1 > USER_STRINGS_MAX)
            return -1;
        string_bytes += length + 1;
        envc++;
    }
    if (envp != NULL && envc == USER_ENV_MAX && envp[envc] != NULL)
        return -1;

    for (int i = envc - 1; i >= 0; i--) {
        int length = string_length_limited(envp[i], USER_STRINGS_MAX) + 1;
        stack -= (uint64_t)length;
        if (copy_to_space(&program->space, stack, envp[i], length) < 0)
            return -1;
        env_addresses[i] = stack;
    }
    for (int i = argc - 1; i >= 0; i--) {
        int length = string_length_limited(argv[i], USER_STRINGS_MAX) + 1;
        stack -= (uint64_t)length;
        if (copy_to_space(&program->space, stack, argv[i], length) < 0)
            return -1;
        argv_addresses[i] = stack;
    }
    uint64_t zero = 0;
    uint64_t metadata_size =
        (uint64_t)(1 + argc + 1 + envc + 1) * sizeof(uint64_t);
    stack = (stack - metadata_size) & ~15ULL;
    program->argv_pointer = stack + sizeof(uint64_t);
    program->envp_pointer =
        program->argv_pointer + (uint64_t)(argc + 1) * sizeof(uint64_t);

    uint64_t argc_value = (uint64_t)argc;
    if (copy_to_space(&program->space, stack, &argc_value,
                      sizeof(argc_value)) < 0)
        return -1;
    if (copy_to_space(&program->space, program->argv_pointer,
                      argv_addresses, argc * sizeof(uint64_t)) < 0 ||
        copy_to_space(&program->space,
                      program->argv_pointer +
                          (uint64_t)argc * sizeof(uint64_t),
                      &zero,
                      sizeof(zero)) < 0)
        return -1;
    if (copy_to_space(&program->space, program->envp_pointer,
                      env_addresses, envc * sizeof(uint64_t)) < 0 ||
        copy_to_space(&program->space,
                      program->envp_pointer +
                          (uint64_t)envc * sizeof(uint64_t),
                      &zero, sizeof(zero)) < 0)
        return -1;
    if (stack < USER_STACK_BOTTOM)
        return -1;
    program->stack_pointer = stack;
    program->argc = argc;
    return 0;
}

static int prepare_program(const char *path, const char *const argv[],
                           const char *const envp[],
                           struct loaded_program *program) {
    uint32_t inode;
    zero_space(&program->space);
    if (minifs_resolve_file(task_current_cwd(), path, &inode) < 0)
        return -1;
    if (load_elf(inode, program) < 0)
        return -1;
    if (build_initial_stack(program, argv, envp) < 0) {
        user_space_destroy(&program->space);
        return -1;
    }
    return 0;
}

int user_space_clone(struct task_address_space *destination,
                     const struct task_address_space *source) {
    if (destination == NULL || source == NULL ||
        create_empty_space(destination) < 0)
        return -1;
    for (uint32_t i = 0; i < source->page_count; i++) {
        if (map_user_page(destination, source->pages[i].va,
                          source->pages[i].flags) < 0) {
            user_space_destroy(destination);
            return -1;
        }
        struct user_page_mapping *mapping =
            find_mapping(destination, source->pages[i].va);
        copy_page(mapping->page, source->pages[i].page);
    }
    return 0;
}

void user_space_destroy(struct task_address_space *space) {
    if (space == NULL)
        return;
    for (uint32_t i = 0; i < space->page_count; i++)
        if (space->pages[i].page != NULL)
            kfree(space->pages[i].page);
    if (space->l0_user != NULL)
        kfree(space->l0_user);
    if (space->l1_mmio != NULL)
        kfree(space->l1_mmio);
    if (space->root != NULL)
        kfree(space->root);
    zero_space(space);
}

int copy_from_user(void *destination, uint64_t source, uint64_t length) {
    struct task_address_space *space = task_current_address_space();
    if (length == 0)
        return 0;
    if (destination == NULL || space == NULL ||
        source + length < source)
        return -1;
    for (uint64_t address = source;
         address < source + length;
         address = (address & ~(PAGE_SIZE - 1ULL)) + PAGE_SIZE) {
        struct user_page_mapping *mapping = find_mapping(space, address);
        if (mapping == NULL || !(mapping->flags & PTE_R))
            return -1;
    }
    return copy_from_space(space, destination, source, length);
}

int copy_to_user(uint64_t destination, const void *source, uint64_t length) {
    struct task_address_space *space = task_current_address_space();
    if (length == 0)
        return 0;
    if (source == NULL || space == NULL ||
        destination + length < destination)
        return -1;
    for (uint64_t address = destination;
         address < destination + length;
         address = (address & ~(PAGE_SIZE - 1ULL)) + PAGE_SIZE) {
        struct user_page_mapping *mapping = find_mapping(space, address);
        if (mapping == NULL || !(mapping->flags & PTE_W))
            return -1;
    }
    return copy_to_space(space, destination, source, length);
}

int copy_string_from_user(char *destination, uint64_t source,
                          uint64_t capacity) {
    if (destination == NULL || capacity == 0)
        return -1;
    for (uint64_t i = 0; i < capacity; i++) {
        if (copy_from_user(&destination[i], source + i, 1) < 0)
            return -1;
        if (destination[i] == '\0')
            return (int)i;
    }
    destination[capacity - 1] = '\0';
    return -1;
}

int user_execve(uint64_t *trap_frame, const char *path,
                const char *const argv[], const char *const envp[]) {
    struct loaded_program program;
    if (trap_frame == NULL ||
        prepare_program(path, argv, envp, &program) < 0)
        return -1;
    struct task_address_space old_space;
    if (task_replace_address_space(&program.space, &old_space) < 0) {
        user_space_destroy(&program.space);
        return -1;
    }
    csr_write(satp, program.space.satp);
    __asm__ volatile("sfence.vma x0, x0");
    user_space_destroy(&old_space);
    for (int i = 0; i < TASK_REG_COUNT; i++)
        trap_frame[i] = 0;
    trap_frame[2] = program.stack_pointer;
    trap_frame[10] = (uint64_t)program.argc;
    trap_frame[11] = program.argv_pointer;
    trap_frame[12] = program.envp_pointer;
    trap_epc_write(program.entry);
    return 0;
}

int user_init(void) {
    static const char *argv[] = {"/bin/shell", NULL};
    static const char *envp[] = {
        "PATH=/bin:/tests",
        "HOME=/",
        "PWD=/",
        NULL
    };
    struct loaded_program program;
    if (prepare_program("/bin/shell", argv, envp, &program) < 0) {
        printk("user_init: cannot load /bin/shell\n");
        return -1;
    }
    if (task_attach_address_space(&program.space) < 0) {
        user_space_destroy(&program.space);
        printk("user_init: no current process\n");
        return -1;
    }
    csr_write(satp, program.space.satp);
    __asm__ volatile("sfence.vma x0, x0");
    uint64_t *context = task_current_initial_trap_context();
    if (context != NULL) {
        context[2] = program.stack_pointer;
        context[10] = program.argc;
        context[11] = program.argv_pointer;
        context[12] = program.envp_pointer;
        task_set_current_entry(program.entry);
    }
#if MINIOS_BOOT_DIAGNOSTICS
    printk("User ELF ready: pid=%d entry=%lx root=%lx\n",
           task_current_pid(), program.entry, (uint64_t)program.space.root);
#endif
    return 0;
}

__attribute__((noreturn))
void enter_user(void) {
    uint64_t status = csr_read(sstatus);
    status &= ~SSTATUS_SPP;
    status |= SSTATUS_SPIE;
    csr_write(sstatus, status);
    csr_write(sepc, task_current_entry());
    trap_scratch_write(task_current_kernel_stack_top());
    uint64_t *context = task_current_initial_trap_context();
    register uint64_t a0 __asm__("a0") = context[10];
    register uint64_t a1 __asm__("a1") = context[11];
    register uint64_t a2 __asm__("a2") = context[12];
    register uint64_t sp_value = context[2];
    __asm__ volatile(
        "mv sp, %0\n"
        "mv a0, %1\n"
        "mv a1, %2\n"
        "mv a2, %3\n"
        "sret\n"
        :
        : "r"(sp_value), "r"(a0), "r"(a1), "r"(a2)
        : "memory"
    );
    __builtin_unreachable();
}
