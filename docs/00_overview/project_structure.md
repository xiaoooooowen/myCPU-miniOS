# MiniOS / myCPU 项目结构图资料

---

## 1. 模拟器 cemu 核心模块

### 整体调用关系

```
main.cpp
  └── Cpu
        ├── Mmu (Sv39 地址翻译)
        ├── Csr (控制状态寄存器)
        ├── Bus (MMIO 地址路由)
        │     ├── Dram (128MB 主存)
        │     ├── Uart (串口)
        │     ├── Clint (定时器)
        │     ├── Plic (中断控制器)
        │     └── BlockDevice (块设备)
        └── Exception (异常/中断)
```

### 各模块详情

| 模块         | 主要职责                                      | 关键源文件                                  | 被谁调用         | 调用谁                       |
|-------------|-----------------------------------------------|---------------------------------------------|------------------|------------------------------|
| **CPU Core** | 取指/执行主循环，寄存器组(32个)，PC管理，异常/中断入口 | [src/cpu.h](file:///home/xiaowen/projects/mycpu/src/cpu.h) [src/cpu.cpp](file:///home/xiaowen/projects/mycpu/src/cpu.cpp) | main.cpp | Mmu, Bus, Csr, InstructionExecutor |
| **CSR**      | 4096个CSR寄存器阵列，M/S模式CSR读写，异常/中断委托判断 | [src/csr.h](file:///home/xiaowen/projects/mycpu/src/csr.h) [src/csr.cpp](file:///home/xiaowen/projects/mycpu/src/csr.cpp) | Cpu, Mmu | (无)                         |
| **MMU**      | Sv39 三级页表遍历，虚拟地址→物理地址翻译，权限检查(U/R/W/X)，A/D位设置 | [src/mmu.h](file:///home/xiaowen/projects/mycpu/src/mmu.h) [src/mmu.cpp](file:///home/xiaowen/projects/mycpu/src/mmu.cpp) | Cpu (fetch/load/store) | Csr, Dram |
| **Bus**      | 物理地址空间路由，将地址分发到 Dram/Uart/Clint/Plic/BlockDevice/TEST_FINISH | [src/bus.h](file:///home/xiaowen/projects/mycpu/src/bus.h) [src/bus.cpp](file:///home/xiaowen/projects/mycpu/src/bus.cpp) | Cpu | Dram, Uart, Clint, Plic, BlockDevice |
| **DRAM**     | 128MB 字节数组主存，支持 8/16/32/64 bit 读写 | [src/dram.h](file:///home/xiaowen/projects/mycpu/src/dram.h) [src/dram.cpp](file:///home/xiaowen/projects/mycpu/src/dram.cpp) | Bus, Mmu | (无)                         |
| **UART**     | NS16550 兼容串口，TX输出到stdout，RX从stdin监听(poll)，支持中断通知 | [src/uart.h](file:///home/xiaowen/projects/mycpu/src/uart.h) [src/uart.cpp](file:///home/xiaowen/projects/mycpu/src/uart.cpp) | Bus | (宿主 stdin/stdout)          |
| **CLINT**    | mtime/mtimecmp 寄存器，tick()推进时间并检测定时器中断 | [src/clint.h](file:///home/xiaowen/projects/mycpu/src/clint.h) [src/clint.cpp](file:///home/xiaowen/projects/mycpu/src/clint.cpp) | Bus, main.cpp | (无)                         |
| **PLIC**     | 平台级中断控制器，pending/senable/spriority/sclaim 寄存器 | [src/plic.h](file:///home/xiaowen/projects/mycpu/src/plic.h) [src/plic.cpp](file:///home/xiaowen/projects/mycpu/src/plic.cpp) | Bus | (无)                         |
| **BlockDevice** | 8MiB 持久化块设备(disk.img)，512B扇区，CMD_READ/CMD_WRITE，即时回写宿主文件 | [src/block_device.h](file:///home/xiaowen/projects/mycpu/src/block_device.h) [src/block_device.cpp](file:///home/xiaowen/projects/mycpu/src/block_device.cpp) | Bus | (宿主文件系统)               |

### 内存地址映射

| 地址范围 | 设备 |
|---|---|
| `0x80000000 - 0x87ffffff` | DRAM (128 MB) |
| `0x02000000 - 0x0200ffff` | CLINT |
| `0x0c000000 - 0x0fffffff` | PLIC |
| `0x10000000 - 0x100000ff` | UART |
| `0x10001000 - 0x100012ff` | BlockDevice |
| `0x00100000 - 0x001000ff` | TEST_FINISH (写入则停机) |

### 指令集支持

- RISC-V RV64I (整数指令集)
- 指令执行器: [src/instructions.h](file:///home/xiaowen/projects/mycpu/src/instructions.h) [src/instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp)
- 指令通过 opcode/funct3/funct7 哈希表分发到对应执行函数
- 支持: LB/LH/LW/LD/LBU/LHU/LWU, SB/SH/SW/SD, 算术/逻辑/移位/分支/跳转, ECALL, FENCE, CSR 读写指令 (CSRRW/CSRRS/CSRRC/CSRRWI/CSRRSI/CSRRCI), MRET/SRET

---

## 2. CPU 执行一条指令的流程

```
main.cpp 主循环 (while true):
  │
  ├─ 1. 检查停机标志
  ├─ 2. CLINT.tick() → 更新 mtime，检测定时器中断 → 设置/清除 MIP.MTIP
  ├─ 3. check_pending_interrupts() → 若 MIP & MIE 有效，handle_interrupt()
  │
  ├─ 4. FETCH:
  │     Cpu::fetch()
  │       → mmu.translate(pc, Instruction, mode)   // 虚拟地址 → 物理地址
  │         → 若 !SATP.SV39: 直通物理地址
  │         → 否则: Sv39 三级页表遍历 (walk)
  │           → Level2 PTE → Level1 PTE → Level0 PTE (4KB) 或 2MB 大页
  │           → 权限检查 (U/R/W/X) + A/D 位更新
  │           → 返回物理地址
  │       → bus.load(paddr, 32)                     // 从物理地址读取 32bit 指令
  │
  ├─ 5. DECODE + EXECUTE:
  │     Cpu::execute(inst)
  │       → InstructionExecutor::execute(cpu, inst)
  │         → 解包 opcode/funct3/funct7
  │         → 哈希表查找对应执行函数 (如 executeLd, executeAddi, executeJal...)
  │         → 执行指令逻辑
  │         → 若为 load/store 指令:
  │             → Cpu::load(addr, size) / Cpu::store(addr, size, value)
  │               → mmu.translate(vaddr, Load/Store, mode)
  │               → bus.load/store(paddr, size, value)
  │                 → 根据物理地址范围路由到 Dram/Uart/Clint/Plic/BlockDevice
  │         → 若为 CSR 指令:
  │             → csr.load(csr_addr) / csr.store(csr_addr, value)
  │         → 若为 ECALL:
  │             → throw Exception(EnvironmentCallFromXMode, pc)
  │         → 若为 MRET/SRET:
  │             → 从 MEPC/SEPC 恢复 PC
  │         → 返回 new_pc (通常 pc+4)
  │
  ├─ 6. PC UPDATE:
  │     cpu.pc = new_pc.value()
  │
  ├─ 7. EXCEPTION HANDLING (catch block):
  │     → handle_exception(e)
  │       → 判断委托: is_medelegated(cause) → S-mode 或 M-mode trap
  │       → 保存: EPC=pc, CAUSE=cause, TVAL=value
  │       → 切换: pc=TVEC, 关中断, 保存特权级, 切换模式
  │       → 若 isFatal(): 停止模拟
```

### 异常类型

| 类型 | cause 编码 | 说明 |
|---|---|---|
| InstructionAddrMisaligned | 0 | 指令地址不对齐 |
| InstructionAccessFault | 1 | 取指访问错误 |
| IllegalInstruction | 2 | 非法指令 |
| Breakpoint | 3 | 断点 |
| LoadAccessFault | 5 | 加载访问错误 |
| StoreAMOAccessFault | 7 | 存储访问错误 |
| EnvironmentCallFromUMode | 8 | U-mode ECALL |
| EnvironmentCallFromSMode | 9 | S-mode ECALL |
| InstructionPageFault | 12 | 指令页错误 |
| LoadPageFault | 13 | 加载页错误 |
| StoreAMOPageFault | 15 | 存储页错误 |

### 中断类型

| 类型 | cause 编码(bit63=1) | 说明 |
|---|---|---|
| Supervisor software | 1 | S-mode 软件中断 |
| Machine software | 3 | M-mode 软件中断 |
| Supervisor timer | 5 | S-mode 定时器中断(委托) |
| Machine timer | 7 | M-mode 定时器中断 |
| Supervisor external | 9 | S-mode 外部中断(委托) |
| Machine external | 11 | M-mode 外部中断 |

---

## 3. MiniOS 启动流程

```
┌─────────────────────────────────────────────────────────────┐
│ cemu boot.bin [--disk disk.img]                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ 1. 创建 Cpu(code, disk_path)                                │
│    ├─ pc = DRAM_BASE (0x80000000)                           │
│    ├─ regs[2] (sp) = DRAM_END (0x87ffffff)                  │
│    ├─ mode = Machine                                        │
│    ├─ 构造 Bus(code, disk_path)                              │
│    │    ├─ Dram(code): 将 boot.bin 写入 DRAM[0x80000000]     │
│    │    └─ BlockDevice(disk_path): 打开/创建 disk.img        │
│    └─ mmu(csr, bus.dram)                                    │
│                                                             │
│ 2. CPU 从 0x80000000 以 M 态开始执行 boot.bin:              │
│                                                             │
│    loader_start.S (_boot_start):                            │
│    ├─ csrw mstatus, zero       // 清状态                     │
│    ├─ la sp, _boot_stack_top   // 设 Bootloader 栈(接近0x80200000) │
│    ├─ csrw mtvec, boot_trap    // M 态 trap 向量             │
│    ├─ 清零 BSS 段                                           │
│    └─ call boot_main           // 进入 loader.c             │
│                                                             │
│    loader.c (boot_main):                                    │
│    ├─ block_read(sector=15360) // 读内核镜像头(扇区15360)     │
│    ├─ 校验: magic("MINIKRNL"), version(1), header_size(64)   │
│    ├─ 校验: load_address == 0x80200000                       │
│    ├─ 校验: image_size > 0 && <= 523776B                     │
│    ├─ 校验: entry 在 [load_addr, load_addr+image_size) 内    │
│    ├─ 逐扇区 block_read(15361..) 读取 kernel.bin             │
│    ├─ 逐字节累加计算 32bit checksum                           │
│    ├─ 校验: checksum == header->checksum                     │
│    ├─ 输出: "[BOOT] checksum OK"                             │
│    └─ kernel_entry() → 跳转到 0x80200000                    │
│                                                             │
│ 3. 内核 start.S (_start):                                   │
│    ├─ csrw mstatus, zero       // 清状态                     │
│    ├─ la sp, _stack_top        // 设内核栈(0x88000000)       │
│    ├─ csrw mtvec, trap_entry_m // M 态 trap 向量             │
│    ├─ 清零 BSS 段                                           │
│    ├─ 配置异常委托 medeleg: bit8(ECALL_U), bit9(ECALL_S)     │
│    ├─ 配置中断委托 mideleg: bit5(STimer)                     │
│    ├─ csrw stvec, trap_entry   // S 态 trap 向量             │
│    ├─ mstatus.MPP = Supervisor  // 下次 mret 进入 S 态       │
│    ├─ csrw mepc, kernel_main   // 返回地址 = kernel_main    │
│    └─ mret → 特权级=MPP=S, PC=MEPC=kernel_main              │
│                                                             │
│ 4. kernel_main (S 态):                                      │
│    ├─ uart_init()                                           │
│    ├─ mem_init()      // bump allocator + 空闲页链表      │
│    ├─ vm_init()       // 建立 Sv39 内核页表(128MB 身份映射) │
│    ├─ minifs_init()   // 挂载 MiniFS v2 文件系统             │
│    ├─ start_shell():                                        │
│    │    ├─ task_set_scheduler(SCHED_RR)                      │
│    │    ├─ task_init()    // 初始化进程子系统(idle pid=0)    │
│    │    ├─ task_create(user_task_entry, "shell")             │
│    │    ├─ timer_init()   // 使能 S 态定时器中断             │
│    │    └─ trap_set_silent(1)  // 减少定时器 trap 日志      │
│    ├─ local_irq_enable()  // 开中断 → 定时器开始推动调度     │
│    └─ 主循环: waitpid(shell_pid, ...) 等待 shell 退出       │
│                                                             │
│ 5. user_task_entry (shell 的入口):                           │
│    └─ user_init()        // 创建用户地址空间，加载 shell ELF │
│       └─ enter_user()    // 进入 U 态执行 /bin/shell         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 内存布局

| 地址范围 | 用途 |
|---|---|
| `0x80000000 - 0x801fffff` | Bootloader (max 2 MiB) |
| `0x80200000 - ...` | MiniOS 内核镜像 + BSS |
| `... - 0x87fbffff` | 物理页分配区 |
| `0x87fc0000 - 0x87ffffff` | 内核栈保护区 (256 KB) |
| `0x88000000` | 内核初始栈顶 |

### 磁盘布局

| 扇区 | 用途 |
|---|---|
| `0 - 37` | MiniFS 超级块、位图、inode 表 |
| `38 - 15359` | MiniFS 数据区 (7.5 MiB) |
| `15360` | 内核镜像头 (64 B) |
| `15361 - 16383` | 内核裸二进制 kernel.bin (max 523776 B) |

---

## 4. MiniOS 内核主要子系统

### 子系统概览

| 子系统 | 关键源文件 | 主要职责 |
|---|---|---|
| **trap** | [os/kernel/trap.S](file:///home/xiaowen/projects/mycpu/os/kernel/trap.S), [os/kernel/trap.c](file:///home/xiaowen/projects/mycpu/os/kernel/trap.c), [os/kernel/trap.h](file:///home/xiaowen/projects/mycpu/os/kernel/trap.h) | 异常/中断入口(trap_entry)，保存/恢复寄存器，分发到 timer/syscall/scheduler |
| **syscall** | [os/kernel/syscall.c](file:///home/xiaowen/projects/mycpu/os/kernel/syscall.c), [os/kernel/syscall.h](file:///home/xiaowen/projects/mycpu/os/kernel/syscall.h) | 系统调用分发(syscall_dispatch)，支持 20+ 个 syscall |
| **timer** | [os/kernel/timer.c](file:///home/xiaowen/projects/mycpu/os/kernel/timer.c), [os/kernel/timer.h](file:///home/xiaowen/projects/mycpu/os/kernel/timer.h) | CLINT 定时器初始化/处理，mtimecmp 设置，时间片管理 |
| **scheduler** | [os/kernel/task.c](file:///home/xiaowen/projects/mycpu/os/kernel/task.c) (sched_tick/select_next等), [os/kernel/task.h](file:///home/xiaowen/projects/mycpu/os/kernel/task.h) | 进程调度(RR/FCFS)，上下文切换 |
| **task** | [os/kernel/task.c](file:///home/xiaowen/projects/mycpu/os/kernel/task.c), [os/kernel/task.h](file:///home/xiaowen/projects/mycpu/os/kernel/task.h) | 进程管理(create/fork/exit/wait)，PCB，进程树，用户地址空间管理 |
| **memory** | [os/kernel/mem.c](file:///home/xiaowen/projects/mycpu/os/kernel/mem.c), [os/kernel/mem.h](file:///home/xiaowen/projects/mycpu/os/kernel/mem.h) | bump allocator + 空闲页链表，kalloc/kfree |
| **vm** | [os/kernel/vm.c](file:///home/xiaowen/projects/mycpu/os/kernel/vm.c), [os/kernel/vm.h](file:///home/xiaowen/projects/mycpu/os/kernel/vm.h) | 内核 Sv39 页表建立(128MB DRAM 身份映射+MMIO映射)，用户页表模板 |
| **MiniFS** | [os/kernel/minifs.c](file:///home/xiaowen/projects/mycpu/os/kernel/minifs.c), [os/kernel/minifs.h](file:///home/xiaowen/projects/mycpu/os/kernel/minifs.h) | 文件系统(v2)，inode/block allocator，目录/文件操作，fd管理 |
| **ELF loader** | [os/kernel/user.c](file:///home/xiaowen/projects/mycpu/os/kernel/user.c) (load_elf等), [os/kernel/user.h](file:///home/xiaowen/projects/mycpu/os/kernel/user.h) | ELF64 解析，PT_LOAD 段加载，用户地址空间构造 |
| **user program** | [os/user/](file:///home/xiaowen/projects/mycpu/os/user/) (shell.c, ls.c, cat.c 等), [os/user/crt0.S](file:///home/xiaowen/projects/mycpu/os/user/crt0.S) | 用户态程序: shell 及外部命令 |

### 子系统调用关系

```
trap_handler (trap.c)
  │
  ├── 中断 (bit63=1):
  │     ├── irq_code=5 (Supervisor timer):
  │     │     ├── timer_handle()        // 重设 mtimecmp
  │     │     └── sched_tick(tf)        // 抢占调度
  │     │           └── switch_from_trap() → save_trap_context / restore_trap_context
  │     └── task_prepare_trap_return()
  │
  └── 异常:
        ├── cause=8 (ECALL from U-mode):
        │     └── syscall_dispatch(tf)    // 系统调用分发
        │           ├── SYS_READ    → minifs_read → block_read
        │           ├── SYS_WRITE   → minifs_write → block_write
        │           ├── SYS_EXIT    → task_exit
        │           ├── SYS_FORK    → user_space_clone + task_fork_from_trap
        │           ├── SYS_EXECVE  → user_execve → load_elf → replace space
        │           ├── SYS_OPEN    → minifs_open
        │           ├── SYS_WAITPID → task_waitpid
        │           └── ...
        │     └── task_reschedule(tf) (若进程变为 ZOMBIE/BLOCKED)
        └── task_prepare_trap_return()

task 模块内部:
  task_create()    → 分配任务槽位 + kalloc 内核栈 + minifs_process_init
  task_fork()      → 复制 trap_ctx + 复制地址空间 + 设置子进程返回0
  task_exit()      → 标记 ZOMBIE + 唤醒等待的父进程 + reparent 子进程
  task_waitpid()   → 回收 ZOMBIE 子进程 + user_space_destroy + 释放槽位
  switch_to()      → 汇编: 保存/恢复 callee-saved 寄存器 (ra, sp, s0-s11)
  yield()          → 协作式让出 CPU (非抢占)

vm 模块:
  vm_init()        → 分配 3 个物理页 (l2, l1_dram, l1_mmio)
                   → 映射: DRAM(128MB, 2MB大页), CLINT, PLIC, UART, TEST_FINISH
                   → 设置 SATP 开启 Sv39
  kernel_l2 / kernel_l1_mmio → 供 user_init 复用为模板创建用户页表

ELF loader (user.c):
  user_init()      → prepare_program("/bin/shell") → load_elf → build_initial_stack
                   → task_attach_address_space → enter_user
  user_execve()    → prepare_program(path) → task_replace_address_space
  user_space_clone() → 深复制所有用户页 (fork 用)
  load_elf()       → 读 ELF header → 遍历 program headers → 映射 PT_LOAD 段
                   → copy_to_space (复制文件内容) → 映射用户栈
  build_initial_stack() → 在用户栈顶构造 argc/argv/envp/strings
```

### 系统调用列表

| 编号 | 名称 | 功能 |
|---|---|---|
| 56 | SYS_OPEN | 打开文件 |
| 57 | SYS_CLOSE | 关闭文件 |
| 61 | SYS_GETDENTS | 读取目录项 |
| 62 | SYS_LSEEK | 文件定位 |
| 63 | SYS_READ | 读文件 |
| 64 | SYS_WRITE | 写文件 |
| 93 | SYS_EXIT | 进程退出 |
| 95 | SYS_WAIT | 等待任意子进程 |
| 124 | SYS_YIELD | 让出 CPU |
| 220 | SYS_FORK | 创建子进程 |
| 221 | SYS_EXECVE | 执行程序 |
| 260 | SYS_WAITPID | 等待指定子进程 |
| 24 | SYS_DUP2 | 复制文件描述符 |
| 34 | SYS_MKDIR | 创建目录 |
| 35 | SYS_UNLINK | 删除文件 |
| 400 | SYS_PS | 查看进程列表 |
| 402 | SYS_CHDIR | 改变目录 |
| 403 | SYS_GETCWD | 获取当前目录 |
| 405 | SYS_KILL | 终止进程 |
| 406 | SYS_SET_CLOEXEC | 设置 close-on-exec |

---

## 5. MiniFS v2 磁盘布局和主要数据结构

### 磁盘布局

```
8 MiB 磁盘 = 16384 个 512B 扇区

┌──────────────────────────────────────────────────┐
│ 扇区 0:         超级块 (1 sector, 512 B)         │
│ 扇区 1:         inode 位图 (1 sector)             │
│ 扇区 2-5:       数据块位图 (4 sectors)            │
│ 扇区 6-37:      inode 表 (32 sectors, 256 inode)  │
│ 扇区 38-15359:  数据区 (15322 sectors, ~7.5 MiB)  │
│ 扇区 15360:     内核镜像头 (64 B, 其余填 0)       │
│ 扇区 15361-16383: kernel.bin (1023 sectors)       │
└──────────────────────────────────────────────────┘
```

### 超级块数据结构

```c
struct minifs_super {
    uint32_t magic;              // 0x4d465332 ("MFS2")
    uint32_t version;            // 2
    uint32_t blocks;             // 15360 (文件系统可用块数)
    uint32_t inode_count;        // 256
    uint32_t inode_bitmap_start; // 1
    uint32_t inode_bitmap_blocks;// 1
    uint32_t data_bitmap_start;  // 2
    uint32_t data_bitmap_blocks; // 4
    uint32_t inode_table_start;  // 6
    uint32_t inode_table_blocks; // 32
    uint32_t data_start;         // 38
    uint32_t max_file_size;      // 65536 (64 KiB)
    uint8_t reserved[464];       // 对齐到 512B
};
```

### inode 数据结构

```c
struct minifs_inode {
    uint32_t mode;                             // 类型+权限 (MINIFS_MODE_FILE=1, MINIFS_MODE_DIR=2, MINIFS_MODE_EXEC=0x100)
    uint32_t size;                             // 文件大小(字节)
    uint32_t parent;                           // 父目录 inode 号
    uint32_t links;                            // 硬链接计数
    uint32_t direct[10];                       // 10 个直接块号
    uint32_t indirect;                         // 1 个一级间接块号 (存 128 个块号)
    uint32_t reserved;                         // 对齐
};
// sizeof = 64 字节，每扇区存 8 个 inode
```

### 目录项数据结构

```c
struct minifs_dirent {
    uint32_t inode;         // 文件 inode 号
    uint32_t mode;          // 类型+权限
    char name[56];          // 文件名 (max 55 字符 + '\0')
};
// sizeof = 64 字节
```

### 内核镜像头

```c
struct boot_image_header {
    uint8_t magic[8];        // "MINIKRNL"
    uint32_t version;        // 1
    uint32_t header_size;    // 64
    uint64_t load_address;   // 0x80200000
    uint64_t entry;          // 0x80200000
    uint32_t image_size;     // kernel.bin 大小
    uint32_t checksum;       // 32bit 累加校验和
    uint8_t reserved[24];
};
// sizeof = 64 字节
```

### 文件描述符管理

```c
// 全局打开文件表 (64 个槽位)
struct open_file {
    int used;         // 是否使用
    int refs;         // 引用计数 (fork 时递增)
    int type;         // OFD_CONSOLE_IN(1) / OFD_CONSOLE_OUT(2) / OFD_FILE(3)
    int flags;        // O_RDONLY / O_WRONLY / O_RDWR / O_CREATE / O_TRUNC / O_APPEND
    uint32_t inode;   // 对应 inode 号
    uint32_t offset;  // 当前读写偏移
};

// 每进程 fd 表 (16 个进程，每进程 16 个 fd)
struct process_fds {
    int used;
    int pid;
    int fd[16];        // fd → 全局 open_file 索引
    uint16_t cloexec;  // close-on-exec 位图
};
```

---

## 6. 用户程序执行链路

```
┌─ Shell 主循环 (user/shell.c: main) ──────────────────────┐
│                                                             │
│ 1. 显示提示符 "minios:/path/> "                             │
│ 2. read_line() 读入命令行                                   │
│ 3. parse_command() 解析命令 (分词/引号/重定向/后台)          │
│                                                             │
│ 4. 内建命令: help / cd / exit / exec / run                  │
│    外部分命令 → fork + execve 方式执行:                      │
│                                                             │
│    pid = fork();                                            │
│    if (pid == 0) {                    // 子进程              │
│        apply_redirections();          // 处理 < > >> 重定向  │
│        try_exec(argv, envp);          // PATH 搜索 + execve  │
│        exit(127);                     // 找不到程序          │
│    }                                                        │
│    if (background)                   // 后台: 打印 [pid N]   │
│        continue;                                             │
│    wait_for(pid);                    // 前台: 等待子进程结束 │
│                                                             │
└─────────────────────────────────────────────────────────────┘

fork() 调用链路:
  user/shell.c: fork()
    → user/libc.c: fork() → __asm__("li a7, 220; ecall")
    → trap_handler → syscall_dispatch → SYS_FORK:
        ├─ user_space_clone(&child, parent)  // 深复制所有用户页
        │    ├─ create_empty_space(&child)   // 复制页表结构
        │    └─ 逐页复制: copy_page + map_user_page
        └─ task_fork_from_trap(tf, child_epc, &child)
             ├─ 分配任务槽位 + 内核栈
             ├─ 复制 trap_ctx (trap_frame → child)
             ├─ child.trap_ctx.regs[10] = 0  // 子进程返回 0
             └─ minifs_process_fork(pid, cpid) // 复制 fd 表

execve() 调用链路:
  user/shell.c: execve(path, argv, envp)
    → user/libc.c: execve() → __asm__("li a7, 221; ecall")
    → trap_handler → syscall_dispatch → SYS_EXECVE:
        └─ user_execve(tf, path, argv, envp)
             ├─ prepare_program(path, argv, envp, &program)
             │    ├─ minifs_resolve_file(cwd, path, &inode)  // 路径解析
             │    ├─ load_elf(inode, &program)                // ELF 加载器
             │    │    ├─ 校验 ELF header (ET_EXEC, RISCV, 64bit, LE)
             │    │    ├─ create_empty_space(&program->space)  // 建用户页表
             │    │    ├─ 遍历 PT_LOAD segments:
             │    │    │    ├─ map_user_page() 逐页映射用户地址空间
             │    │    │    └─ copy_to_space() 复制 segment 内容
             │    │    └─ 映射用户栈 (4 页, USER_STACK_BOTTOM → USER_STACK_TOP)
             │    └─ build_initial_stack(&program, argv, envp)  // 构造初始栈
             │         ├─ 栈顶布局: [argc][argv[]][0][envp[]][0][strings...]
             │         └─ 16 字节对齐
             ├─ task_replace_address_space(&program->space, &old_space)
             │    └─ 原子替换当前进程地址空间
             └─ user_space_destroy(&old_space)  // 销毁旧地址空间

用户态运行:
  enter_user() → sret 进入 U-mode
  crt0.S (_start):
    ├─ ld a0, 0(sp)           // argc
    ├─ addi a1, sp, 8         // argv
    ├─ slli t0, a0, 3; add a2  // envp = sp + 8 + argc*8 + 8
    ├─ call main(argc, argv, envp)
    └─ main 返回后: li a7, 93; ecall  → 调用 exit()

用户程序退出:
  main() 返回 → exit() → __asm__("li a7, 93; ecall")
    → trap_handler → syscall_dispatch → SYS_EXIT:
        └─ sys_exit(code)
             ├─ task_exit(code)    // 置 ZOMBIE, 关闭 fd, 唤醒父进程
             └─ 若 init 进程退出 → 写 TEST_FINISH 停机

Shell 回收:
  waitpid(shell_pid, ...) (在 kernel_main 主循环)
    → trap_handler → syscall_dispatch → SYS_WAITPID:
        └─ task_waitpid(pid, &status, nohang)
             ├─ 遍历子进程找到 ZOMBIE
             ├─ user_space_destroy(&child->address_space)
             ├─ minifs_close_all(pid)
             └─ 释放任务槽位
```

### 用户地址空间布局

| 地址范围 | 用途 |
|---|---|
| `0x00010000 - USER_STACK_BOTTOM` | 代码段 + 数据段 (ELF PT_LOAD 加载) |
| `USER_STACK_BOTTOM - 0x00200000` | 用户栈 (4 页, 16 KiB) |
| 最大值 | `USER_MAX_PAGES=64` 页 (256 KiB) |

---

## 附录: 关键源文件索引

### cemu 模拟器 (C++)

| 文件 | 说明 |
|---|---|
| [src/main.cpp](file:///home/xiaowen/projects/mycpu/src/main.cpp) | 模拟器入口，主循环 |
| [src/cpu.h](file:///home/xiaowen/projects/mycpu/src/cpu.h) | CPU 类定义(寄存器/PC/模式) |
| [src/cpu.cpp](file:///home/xiaowen/projects/mycpu/src/cpu.cpp) | CPU fetch/execute/异常处理 |
| [src/instructions.h](file:///home/xiaowen/projects/mycpu/src/instructions.h) | 指令执行器接口 |
| [src/instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp) | 全部 RV64I 指令实现 |
| [src/mmu.h](file:///home/xiaowen/projects/mycpu/src/mmu.h) | MMU 类定义 |
| [src/mmu.cpp](file:///home/xiaowen/projects/mycpu/src/mmu.cpp) | Sv39 三级页表遍历 |
| [src/csr.h](file:///home/xiaowen/projects/mycpu/src/csr.h) | CSR 控制器类定义 |
| [src/csr.cpp](file:///home/xiaowen/projects/mycpu/src/csr.cpp) | CSR 读写 + 委托判断 |
| [src/bus.h](file:///home/xiaowen/projects/mycpu/src/bus.h) | 总线类定义 |
| [src/bus.cpp](file:///home/xiaowen/projects/mycpu/src/bus.cpp) | 地址路由分发 |
| [src/dram.h](file:///home/xiaowen/projects/mycpu/src/dram.h) | DRAM 类定义 |
| [src/dram.cpp](file:///home/xiaowen/projects/mycpu/src/dram.cpp) | 128MB 主存实现 |
| [src/uart.h](file:///home/xiaowen/projects/mycpu/src/uart.h) | UART 类定义(NS16550) |
| [src/uart.cpp](file:///home/xiaowen/projects/mycpu/src/uart.cpp) | UART 实现(stdin/stdout) |
| [src/clint.h](file:///home/xiaowen/projects/mycpu/src/clint.h) | CLINT 定时器定义 |
| [src/clint.cpp](file:///home/xiaowen/projects/mycpu/src/clint.cpp) | mtime/mtimecmp 实现 |
| [src/plic.h](file:///home/xiaowen/projects/mycpu/src/plic.h) | PLIC 中断控制器定义 |
| [src/plic.cpp](file:///home/xiaowen/projects/mycpu/src/plic.cpp) | PLIC 实现 |
| [src/block_device.h](file:///home/xiaowen/projects/mycpu/src/block_device.h) | 块设备类定义 |
| [src/block_device.cpp](file:///home/xiaowen/projects/mycpu/src/block_device.cpp) | 8MiB 持久化块设备 |
| [src/exception.h](file:///home/xiaowen/projects/mycpu/src/exception.h) | 异常类定义 |
| [src/exception.cpp](file:///home/xiaowen/projects/mycpu/src/exception.cpp) | 异常实现 |
| [src/param.h](file:///home/xiaowen/projects/mycpu/src/param.h) | 地址常量/掩码定义 |

### MiniOS 内核 (C + RISC-V 汇编)

| 文件 | 说明 |
|---|---|
| [os/boot/loader_start.S](file:///home/xiaowen/projects/mycpu/os/boot/loader_start.S) | Bootloader M 态入口 |
| [os/boot/loader.c](file:///home/xiaowen/projects/mycpu/os/boot/loader.c) | 内核镜像加载/校验/跳转 |
| [os/boot/loader.ld](file:///home/xiaowen/projects/mycpu/os/boot/loader.ld) | Bootloader 链接脚本 |
| [os/boot/start.S](file:///home/xiaowen/projects/mycpu/os/boot/start.S) | 内核 M 态入口(start.S) |
| [os/kernel/kernel.c](file:///home/xiaowen/projects/mycpu/os/kernel/kernel.c) | kernel_main 启动流程 |
| [os/kernel/trap.S](file:///home/xiaowen/projects/mycpu/os/kernel/trap.S) | 异常/中断汇编入口(trap_entry) |
| [os/kernel/trap.c](file:///home/xiaowen/projects/mycpu/os/kernel/trap.c) | trap_handler C 处理函数 |
| [os/kernel/syscall.c](file:///home/xiaowen/projects/mycpu/os/kernel/syscall.c) | 系统调用分发 |
| [os/kernel/task.c](file:///home/xiaowen/projects/mycpu/os/kernel/task.c) | 进程管理+调度器 |
| [os/kernel/switch.S](file:///home/xiaowen/projects/mycpu/os/kernel/switch.S) | switch_to 上下文切换 |
| [os/kernel/timer.c](file:///home/xiaowen/projects/mycpu/os/kernel/timer.c) | CLINT 定时器驱动 |
| [os/kernel/mem.c](file:///home/xiaowen/projects/mycpu/os/kernel/mem.c) | 物理内存分配器 |
| [os/kernel/vm.c](file:///home/xiaowen/projects/mycpu/os/kernel/vm.c) | 内核 Sv39 页表建立 |
| [os/kernel/user.c](file:///home/xiaowen/projects/mycpu/os/kernel/user.c) | ELF 加载器+用户地址空间管理 |
| [os/kernel/minifs.c](file:///home/xiaowen/projects/mycpu/os/kernel/minifs.c) | MiniFS v2 文件系统 |
| [os/kernel/block.c](file:///home/xiaowen/projects/mycpu/os/kernel/block.c) | 内核端块设备驱动 |
| [os/kernel/sync.c](file:///home/xiaowen/projects/mycpu/os/kernel/sync.c) | 信号量/互斥锁 |
| [os/kernel/uart.c](file:///home/xiaowen/projects/mycpu/os/kernel/uart.c) | 内核 UART 驱动 |
| [os/kernel/printk.c](file:///home/xiaowen/projects/mycpu/os/kernel/printk.c) | 内核 printf |
| [os/kernel/ramfs.c](file:///home/xiaowen/projects/mycpu/os/kernel/ramfs.c) | 内存文件系统(仅启动诊断用) |
| [os/include/csr.h](file:///home/xiaowen/projects/mycpu/os/include/csr.h) | 内核 CSR 访问宏+常量 |
| [os/linker.ld](file:///home/xiaowen/projects/mycpu/os/linker.ld) | 内核链接脚本 |

### 用户程序

| 文件 | 说明 |
|---|---|
| [os/user/shell.c](file:///home/xiaowen/projects/mycpu/os/user/shell.c) | Shell (fork+execve 执行命令) |
| [os/user/crt0.S](file:///home/xiaowen/projects/mycpu/os/user/crt0.S) | 用户程序入口(argc/argv/envp → main) |
| [os/user/libc.c](file:///home/xiaowen/projects/mycpu/os/user/libc.c) | 用户态 C 库(syscall 封装) |
| [os/user/ls.c](file:///home/xiaowen/projects/mycpu/os/user/ls.c) | ls 命令 |
| [os/user/cat.c](file:///home/xiaowen/projects/mycpu/os/user/cat.c) | cat 命令 |
| [os/user/echo.c](file:///home/xiaowen/projects/mycpu/os/user/echo.c) | echo 命令 |
| [os/user/pwd.c](file:///home/xiaowen/projects/mycpu/os/user/pwd.c) | pwd 命令 |
| [os/user/ps.c](file:///home/xiaowen/projects/mycpu/os/user/ps.c) | ps 命令 |
| [os/user/kill.c](file:///home/xiaowen/projects/mycpu/os/user/kill.c) | kill 命令 |
| [os/user/env.c](file:///home/xiaowen/projects/mycpu/os/user/env.c) | env 命令 |
| [os/user/mkdir.c](file:///home/xiaowen/projects/mycpu/os/user/mkdir.c) | mkdir 命令 |
| [os/user/rm.c](file:///home/xiaowen/projects/mycpu/os/user/rm.c) | rm 命令 |
| [os/user/touch.c](file:///home/xiaowen/projects/mycpu/os/user/touch.c) | touch 命令 |
| [os/user/write.c](file:///home/xiaowen/projects/mycpu/os/user/write.c) | write 命令 |
| [os/user/spin.c](file:///home/xiaowen/projects/mycpu/os/user/spin.c) | 测试程序(spin) |
| [os/user/fstest.c](file:///home/xiaowen/projects/mycpu/os/user/fstest.c) | 文件系统测试 |
| [os/user/forktest.c](file:///home/xiaowen/projects/mycpu/os/user/forktest.c) | fork 测试 |
| [os/user/argtest.c](file:///home/xiaowen/projects/mycpu/os/user/argtest.c) | 参数测试 |
| [os/user/testall.c](file:///home/xiaowen/projects/mycpu/os/user/testall.c) | 一键测试套件(顺序执行 argtest+forktest+fstest) |

### 工具脚本

| 文件 | 说明 |
|---|---|
| [tools/mkfs_minifs.py](file:///home/xiaowen/projects/mycpu/tools/mkfs_minifs.py) | 创建 MiniFS 磁盘镜像 |
| [tools/install_kernel.py](file:///home/xiaowen/projects/mycpu/tools/install_kernel.py) | 安装/更新内核槽+旧磁盘迁移 |
| [os/Makefile](file:///home/xiaowen/projects/mycpu/os/Makefile) | 构建+运行入口 |
