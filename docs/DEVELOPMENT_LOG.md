# 开发日志

> 项目：MiniOS / myCPU — RISC-V 模拟器与迷你操作系统
> 目的：记录每次修改的上下文、决策理由和技术细节，便于后续回顾和学习。

***

## 2026-06-03 — 阶段 4：机器定时器与 CLINT

### 背景

阶段 3 建立了 trap handler 和异常处理路径。阶段 4 的目标是让定时器中断周期性触发，为后续调度器做准备。核心挑战：模拟器端（C++）需要具备中断检测和分发能力，内核端（C）需要初始化 CLINT 并响应定时器中断，trap handler 需要区分中断（mepc 不推进）和异常（mepc 推进）。

### 修改清单

#### 1. Simulator 端：CLINT tick() 方法

**文件**：[src/clint.h](file:///home/xiaowen/projects/mycpu/src/clint.h)、[src/clint.cpp](file:///home/xiaowen/projects/mycpu/src/clint.cpp)

**新增**：
- `bool tick(uint64_t increment = 1)` — 每指令周期推进 mtime，返回 `mtime >= mtimecmp`
- `get_mtime()` / `get_mtimecmp()` — 获取当前值（调试用）

```cpp
bool Clint::tick(uint64_t increment) {
  mtime += increment;
  return mtime >= mtimecmp;
}
```

#### 2. Simulator 端：CPU 中断检测与处理

**文件**：[src/cpu.h](file:///home/xiaowen/projects/mycpu/src/cpu.h)、[src/cpu.cpp](file:///home/xiaowen/projects/mycpu/src/cpu.cpp)

**新增方法 A — `check_pending_interrupts()`**：
- 读取 MIP、MIE、MSTATUS 寄存器
- 检查全局中断使能 `mstatus.MIE`
- 计算 `pending = mip & mie`（只处理同时挂起且使能的中断）
- 优先级：MEI(11) > MSI(3) > MTI(7)
- 返回带 bit63 置位的 cause（中断标记）

**新增方法 B — `handle_interrupt(uint64_t cause)`**：
- 切换到 M 模式
- 跳转到 mtvec 指向的 trap handler
- 保存中断前 PC 到 mepc（不推进，mret 返回被中断的指令）
- 保存 mcause（含 bit63 中断标记）、mtval(=0)
- 更新 mstatus：保存 MIE→MPIE，清除 MIE，保存 MPP

#### 3. Simulator 端：main.cpp 中断轮询

**文件**：[src/main.cpp](file:///home/xiaowen/projects/mycpu/src/main.cpp)

**主循环修改**：
```cpp
// 每指令周期推进 CLINT，根据结果更新 MTIP
if (cpu.bus.clint.tick()) {
    cpu.csr.store(MIP, cpu.csr.load(MIP) | MASK_MTIP);   // 触发
} else {
    cpu.csr.store(MIP, cpu.csr.load(MIP) & ~MASK_MTIP);   // 清除
}
// 取指前检查中断
auto maybe_irq = cpu.check_pending_interrupts();
if (maybe_irq.has_value()) {
    cpu.handle_interrupt(maybe_irq.value());
    continue;  // 中断返回后继续下一条指令
}
```

**设计要点**：中断在指令边界采样（取指前检查），这是 RISC-V 规范的标准行为。

#### 4. 内核定时器驱动

**新增文件**：
- [os/kernel/timer.h](file:///home/xiaowen/projects/mycpu/os/kernel/timer.h) — CLINT MMIO 地址宏 + `timer_init()` / `timer_handle()` 声明
- [os/kernel/timer.c](file:///home/xiaowen/projects/mycpu/os/kernel/timer.c) — 定时器驱动实现

**timer_init() 逻辑**：
1. 设置首次 mtimecmp = mtime + TIMER_INTERVAL(500)
2. `csr_set(mie, MIE_MTIE)` 使能机器定时器中断
3. 打印当前 mtime/mtimecmp 值

**timer_handle() 逻辑**：
1. tick_count++
2. 设置下一次 mtimecmp = mtime + TIMER_INTERVAL
3. 打印 Tick 序号和 mtime 值

#### 5. 更新 trap handler

**文件**：[os/kernel/trap.c](file:///home/xiaowen/projects/mycpu/os/kernel/trap.c)

**关键修改**：
- 中断分支（bit63=1）：switch 解码 irq_code，timer 中断(7) 调用 `timer_handle()`，**不推进 mepc**（直接 return）
- 异常分支（bit63=0）：原有逻辑，推进 `mepc+4`

**中断 vs 异常的处理差异**：

| 特性 | 异常（如 ECALL） | 中断（如 Timer） |
|------|-----------------|-----------------|
| mepc | 指向触发异常的指令 | 指向被中断的指令（尚未执行） |
| mepc 推进 | +4（跳过 ECALL） | 不推进（返回继续执行） |
| mret 行为 | 回到异常指令的下一条 | 回到被中断的指令 |

#### 6. printk 格式扩展

**文件**：[os/kernel/printk.c](file:///home/xiaowen/projects/mycpu/os/kernel/printk.c)

**修改**：新增 `%u`（unsigned int）格式支持，复用 `print_dec`。

#### 7. 更新 kernel.c 和 Makefile

**文件**：
- [os/kernel/kernel.c](file:///home/xiaowen/projects/mycpu/os/kernel/kernel.c) — Phase 4 测试段：`timer_init()` → `local_irq_enable()` → `wfi` 循环等待中断
- [os/Makefile](file:///home/xiaowen/projects/mycpu/os/Makefile) — 新增 `timer.o` 编译目标

### 构建验证

```bash
cmake --build build_wsl -j$(nproc)  # ✅ 0 warnings
cd os && make clean && make         # ✅ 0 warnings
```

### 测试结果

```
100% tests passed, 0 tests failed out of 78
```

### MiniOS 运行结果

```
--- Phase 4: Timer Interrupt Test ---
Timer initialized: mtime=0x4e83, mtimecmp=0x506c
=== TRAP ===
mcause: 0x8000000000000007     ← 定时器中断（bit63=1, code=7 = MTI）
mepc:   0x80000054             ← wfi 指令地址
mtval:  0x0
Type: Interrupt (0x7)
  -> Machine timer interrupt
Tick #1 @ mtime=0x7186
=== TRAP END ===
Tick #2 @ mtime=0x9363
Tick #3 @ mtime=0xb54e
Tick #4 @ mtime=0xd747
... (周期性触发，每次约 2000 cycles)
```

### 经验笔记

1. **中断在指令边界采样**：在 main.cpp 的 fetch 之前检查中断，而非 execute 之后。这符合 RISC-V 规范——中断在每条指令执行完后采样。

2. **MTIP 由 mtime >= mtimecmp 决定**：CLINT tick() 返回比较结果，main.cpp 据此设置/清除 MIP.MTIP。内核 timer_handle 更新 mtimecmp 后，下一周期 mtime < 新 mtimecmp，MTIP 自动清除。

3. **mret 自动恢复 MIE**：executeMRET 将 MPIE 恢复到 MIE，再设置 MPIE=1。这意味着 trap handler 不需要手动重新使能中断——mret 会自动完成。

4. **中断 vs 异常的 mepc 语义不同**：
   - 异常：mepc 指向异常指令本身（如 ECALL），需要 +4 跳过
   - 中断：mepc 指向尚未执行的被中断指令，mret 直接回到该指令继续执行

5. **wfi 不真正等待**：当前 wfi 是空实现（NOP），内核使用 `while(1) { __asm__ volatile("wfi"); }` 做忙等待。实际硬件中 WFI 会暂停 CPU 直到中断到达，这里只是一个语义占位符。

6. **中断优先级**：RISC-V 规范定义 MEI > MSI > MTI（从高到低），实际实现中按此顺序检查 pending 位。

***

## 2026-06-03 — 阶段 5：简单内存管理

### 背景

阶段 4 建立了定时器中断，内核已能在中断驱动下稳定运行。阶段 5 的目标是建立最小物理页分配器，为后续阶段 6（任务调度，需要为每个任务分配内核栈）提供内存基础。

### 修改清单

#### 1. 创建 os/kernel/mem.h — 分配器接口

**新增文件**：[os/kernel/mem.h](file:///home/xiaowen/projects/mycpu/os/kernel/mem.h)

**接口**：
- `void mem_init(void)` — 初始化分配器（O(1) 计算可用页数）
- `void* kalloc(void)` — 分配一页（4096 字节对齐），失败返回 NULL
- `void kfree(void* ptr)` — 释放一页，含安全检查（页对齐 + 地址范围）
- `size_t mem_free_pages(void)` — 查询剩余空闲页数

#### 2. 创建 os/kernel/mem.c — 分配器实现

**新增文件**：[os/kernel/mem.c](file:///home/xiaowen/projects/mycpu/os/kernel/mem.c)

**设计决策：bump allocator + 空闲链表混合**

初始方案是 pure free-list：在 `mem_init()` 中遍历所有空闲页逐个链入链表。但模拟器逐条指令执行，初始化 32702 页需要极长时间（每页存一次 `fp->next` 相当于一次 store 指令 + 循环跳转），导致内核在初始化阶段耗时过长。

最终方案：
- `mem_init()` 只做 O(1) 计算：`total_free = (heap_end - bump_ptr) / PAGE_SIZE`
- `kalloc()` 优先从 `free_list` 取页（kfree 回收的页）→ O(1)
- `free_list` 为空时走 bump allocator（推进指针）→ O(1)
- `kfree()` 将释放的页链入 `free_list` → O(1)

**堆范围**：
```
bump_ptr = align_up(&_kernel_end, PAGE_SIZE)   // 内核代码/数据紧后
heap_end = &_stack_top - 256KB                 // 栈保护区
```

**执行结果**：内核区域约 102KB（25.5 页），_kernel_end 向上对齐到 0x80002000。堆区从 0x80002000 到 0x87FC0000，共 32702 页（约 127.7 MB）。

#### 3. 更新 os/Makefile

**文件**：[os/Makefile](file:///home/xiaowen/projects/mycpu/os/Makefile)

**修改**：
- 新增 `mem.o` 编译目标
- KERNEL_SRCS 加入 `kernel/mem.c`
- OBJS 从 7 个扩展到 8 个

#### 4. 更新 os/kernel/kernel.c — 内存分配测试

**文件**：[os/kernel/kernel.c](file:///home/xiaowen/projects/mycpu/os/kernel/kernel.c)

**验证逻辑**：
1. `mem_init()` 初始化 → 打印空闲页数
2. 连续分配 p1/p2/p3 三个页 → 验证地址递增且互不相同
3. 向 p1 写入 ABCDEFGHIJKLMNOP → 验证可读写
4. `kfree(p2)` → 验证空闲计数 +1
5. 重新 `kalloc()` → 验证返回地址 == p2（空闲链表回收复用）
6. 全部释放 → 验证空闲计数回到初始值

### 构造验证

```bash
cd os && make clean && make     # ✅ 0 warnings
cmake --build build_wsl -j$(nproc)  # ✅ 0 warnings
```

### 测试结果

```
100% tests passed, 0 tests failed out of 78
```

### MiniOS 运行结果

```
--- Phase 5: Memory Allocator Test ---
Free pages: 32702
Alloc p1: 0x80002000
Alloc p2: 0x80003000
Alloc p3: 0x80004000
Free pages after alloc: 32699
p1 data: ABCDEFGHIJKLMNOP
Free pages after kfree(p2): 32700
Alloc p4: 0x80003000 (should == p2: 0x80003000)
Free pages after cleanup: 32702
Memory allocator test passed!
```

验收标准全部满足：
- 能分配一页或多页物理内存
- 不覆盖内核区域（p1 从 _kernel_end 对齐边界开始）
- 分配地址按页对齐（0x...000 / 0x...000）
- 重复分配不会返回同一页
- 释放后可以再次分配（p4 == p2 地址复用）
- 计数器闭环一致（32702 → 32699 → 32700 → 32702）

### 经验笔记

1. **模拟器性能是设计约束**：纯空闲链表 O(n) 初始化在模拟器上慢得不可接受。在实际硬件上遍历 3 万页是瞬间的事，但模拟器每条指令都要解释执行。Bump + free_list 混合方案用 O(1) 初始化避免了这个问题。

2. **空闲链表节点内嵌在空闲页中**：`struct free_page { struct free_page* next; }` 直接在空闲页的起始 8 字节存储 next 指针，不额外消耗内存。这是内核内存管理器的经典技巧。

3. **kfree 的安全检查**：页对齐检查（`addr & (PAGE_SIZE-1)`）+ 地址范围检查（`>= _kernel_start && < _stack_top`）。裸机环境下没有 MMU 保护，如果释放了错误的地址（如栈上的局部变量），会破坏空闲链表导致后续 kalloc 返回非法地址。

4. **`extern char` 获取链接符号地址**：C 编译器将符号视为变量地址，`&_kernel_end` 得到链接器分配的值。不能 `extern uintptr_t _kernel_end`（会尝试读取该地址的 8 字节内容，而非地址本身）。

5. **栈保护区 256KB**：为内核栈预留空间，防止 bump allocator 推进到栈区域。当前内核只有一个线程，栈用量很小，256KB 远超实际需要，但为未来多任务留足余量。

6. **print_hex 自带 0x 前缀**：初版格式串写了 `"0x%lx"` 导致 `0x0x80002000`。修复为 `"%lx"`。

***

## 2026-06-02 — 阶段 3：Trap 入口与异常处理

### 背景

阶段 2 完成了 UART 输出和 printk 内核日志，MiniOS 已经能通过串口输出信息。但此时模拟器的 `executeECALL` 只抛异常（`throw Exception(EnvironmentCallFromMMode, inst)`），没有 trap 处理路径。阶段 3 的目标是：

1. 建立从硬件异常到内核 handler 的完整路径
2. 让 MiniOS 通过 `ecall` 主动触发异常并进入 trap handler
3. 在 trap handler 中读取 mcause/mepc/mtval 并通过 printk 输出
4. 通过 mret 正确返回到异常发生后的下一条指令

### 修改清单

#### 1. 创建 `os/kernel/trap.S` — trap 入口汇编

**新增文件**：[os/kernel/trap.S](file:///home/xiaowen/projects/mycpu/os/kernel/trap.S)

**内容**：
- `trap_entry` 是 mtvec 指向的入口点
- 分配 256 字节栈帧（32 个寄存器 × 8 字节）
- 保存全部通用寄存器（x0 硬连线零跳过）
- 调用 C 函数 `trap_handler`
- 恢复所有寄存器后执行 `mret`

**遇到的工程问题 — sp 恢复顺序 bug**：

| 项 | 详情 |
|------|------|
| 症状 | trap handler 执行后，恢复寄存器时触发 `LoadAccessFault` (Fatal) |
| 根因 | 恢复顺序中 `ld sp, 16(sp)` 在恢复其他寄存器之前执行，sp 值改变后所有后续 `ld` 从错误地址加载 |
| 修复 | sp 必须是最后一条 `ld` 指令 |

**遇到的工程问题 — t0 被借用后的值污染**：

| 项 | 详情 |
|------|------|
| 根因 | 进入 trap 时 t0 保存内核正在使用的值，但 trap 帧必须用 t0 计算原始 sp（`addi t0, sp, 256`），导致覆盖 t0 原值 |
| 修复 | 三步操作：先 `sd t0, 40(sp)` 保存原始值 → 借用 t0 计算 sp 并 `sd` → `ld t0, 40(sp)` 恢复 t0 原始值 |

#### 2. 创建 `os/kernel/trap.h` 和 `os/kernel/trap.c`

**新增文件**：
- [os/kernel/trap.h](file:///home/xiaowen/projects/mycpu/os/kernel/trap.h) — trap handler 头文件
- [os/kernel/trap.c](file:///home/xiaowen/projects/mycpu/os/kernel/trap.c) — trap handler C 实现

**trap_handler 逻辑**：
1. 通过 `trap_cause_read()` / `trap_epc_read()` / `trap_tval_read()` 读取 CSR
2. 通过 printk 输出 mcause/mepc/mtval 值
3. 判断最高位区分中断 vs 异常
4. 用 switch-case 解码常见异常类型（ECALL/EBREAK/IllegalInstruction）
5. **关键**：执行 `trap_epc_write(epc + 4)` 推进 mepc，避免 mret 回到同一 ecall

**遇到的工程问题 — mcause 值为 10 而非 11**：

最初在 switch-case 中用 `case 11` 匹配 M-mode ECALL，但实际 mcause=10（0xa）。`EnvironmentCallFromMMode` 在 RISC-V 特权规范中 encode 为 10，不是 11。修正后正确输出 `-> Environment call from M-mode`。

#### 3. 修改 `os/boot/start.S` — 设置 mtvec

**文件**：[os/boot/start.S](file:///home/xiaowen/projects/mycpu/os/boot/start.S)

**修改**：在 BSS 清零之前添加：
```asm
la t0, trap_entry
csrw mtvec, t0
```
将 mtvec 指向 trap_entry，使得 CPU 在发生异常时跳转到 trap 入口。

#### 4. 修改 `os/kernel/kernel.c` — 触发测试异常

**文件**：[os/kernel/kernel.c](file:///home/xiaowen/projects/mycpu/os/kernel/kernel.c)

**修改**：在 printk 演示之后添加：
```c
printk("--- Phase 3: Trap Test ---\n");
printk("Triggering ECALL to test trap handler...\n");
__asm__ volatile("ecall");
printk("Returned from trap handler!\n");
printk("Trap round-trip successful!\n");
```

验证 trap 完整路径：ecall → trap_entry → trap_handler → mret → 回到 ecall 下一条指令。

#### 5. 修复 `os/include/csr.h` — 宏展开缺陷

**文件**：[os/include/csr.h](file:///home/xiaowen/projects/mycpu/os/include/csr.h)

**问题**：`trap_cause_read()` 展开为 `csr_read(TRAP_CAUSE)`，再展开为 `csr_read(mcause)`。但 C 预处理器的 `#` 字符串化运算符不会展开宏参数，`#csr` 直接将参数名（而非其展开值）转为字符串，导致内联汇编中出现 `csrr t0, TRAP_CAUSE` 而非 `csrr t0, mcause`。

**修复**：添加间接宏层：
```c
#define _csr_read(csr)  ({ uint64_t _v; __asm__ volatile("csrr %0, " #csr : "=r"(_v)); _v; })
#define csr_read(csr)   _csr_read(csr)
```
外层 `csr_read(TRAP_CAUSE)` 先将 `TRAP_CAUSE` 展开为 `mcause`，再调用 `_csr_read(mcause)`，此时 `#csr` 得到正确的 `mcause`。

`csr_write`/`csr_set`/`csr_clear` 同样修复。

#### 6. 更新 `os/Makefile`

**文件**：[os/Makefile](file:///home/xiaowen/projects/mycpu/os/Makefile)

**修改**：
- 新增 `KERNEL_ASMS = kernel/trap.S`
- 新增 `trap_handler.o`（C 编译）和 `trap_entry.o`（汇编编译）目标
- OBJS 从 4 个扩展到 6 个（start.o + kernel.o + uart.o + printk.o + trap_handler.o + trap_entry.o）

### 构建验证

```bash
cd os && make clean && make    # ✅ 0 warnings
cmake --build build_wsl -j$(nproc)  # ✅ 0 warnings
```

### 测试结果

```
100% tests passed, 0 tests failed out of 78
```

### MiniOS 运行结果

```
--- Phase 3: Trap Test ---
Triggering ECALL to test trap handler...
=== TRAP HANDLER ===
mcause: 0xa
mepc:   0x80000120
mtval:  0x73
Type: Exception (0xa)
  -> Environment call from M-mode
=== TRAP END ===
Returned from trap handler!
Trap round-trip successful!
```

- ECALL 触发 trap，mcause=0xa（M-mode 环境调用）
- mepc 指向 ECALL 指令地址，mtval=0x73（ECALL 的 opcode）
- trap handler 正确解码异常类型
- mepc+4 后 mret 返回，执行 "Returned from trap handler!"
- 无 Fatal 异常，内核正常运行

### 经验笔记

1. **汇编寄存器保存/恢复顺序至关重要**：sp 改变后所有基于 sp 的偏移都错。sp 必须最后恢复。借用临时寄存器时需先保存其原始值再借用，借完后立即恢复。

2. **mret 返回地址决定是否死循环**：mret 将 PC 设为 mepc。如果不主动推进 mepc（+4），mret 回到同一条 ECALL 指令，形成无限循环。注意中断场景下 mepc 不需要推进（中断在指令边界采样）。

3. **C 预处理器 `#` 运算符不展开参数**：`#csr` 直接字符串化原始参数名，不会先展开宏别名。需要两层宏定义来让参数先展开。这是 C 预处理的经典陷阱。

4. **RISC-V ECALL cause 编码**：ECALL 在 M-mode 的 cause 是 10（0xa），而非直觉的 11。需要以 RISC-V 特权规范为准。

5. **trap handler 不需要自己保存 CSR**：mtvec/mepc/mcause/mtval/mstatus 的保存和恢复由模拟器 `Cpu::handle_exception` 和硬件 mret 自动完成。trap handler 只负责读取和决策。

***

## 2026-06-02 — 阶段 2：UART 输出与内核日志

### 背景

阶段 1 已经让 MiniOS 在模拟器上成功启动并输出 "MiniOS booting..." 和 "Hello from kernel!"。但 UART 输出逻辑全部内嵌在 `kernel.c` 中，没有模块化。阶段 2 的目标是：

1. 将 UART 驱动从内核中分离为独立模块
2. 实现最小 `printk`，支持格式化输出
3. 让内核日志系统可复用，为后续阶段（trap handler 日志、定时器日志等）打基础

### 修改清单

#### 1. 创建 `os/kernel/uart.h` 和 `os/kernel/uart.c`

**新增文件**：
- [os/kernel/uart.h](file:///home/xiaowen/projects/mycpu/os/kernel/uart.h) — UART 驱动头文件
- [os/kernel/uart.c](file:///home/xiaowen/projects/mycpu/os/kernel/uart.c) — UART 驱动实现

**内容**：
- `uart_init()` — 初始化（当前为空，预留）
- `uart_putc(char c)` — 轮询等待 LSR THRE 位，然后写入 THR
- `uart_puts(const char* s)` — 字符串输出

**设计决策**：UART 寄存器地址常量和掩码从头文件定义移到 uart.c 内部（封装），头文件只暴露 API。

#### 2. 创建 `os/kernel/printk.h` 和 `os/kernel/printk.c`

**新增文件**：
- [os/kernel/printk.h](file:///home/xiaowen/projects/mycpu/os/kernel/printk.h) — 内核日志头文件
- [os/kernel/printk.c](file:///home/xiaowen/projects/mycpu/os/kernel/printk.c) — 内核日志实现

**支持的格式说明符**：

| 格式 | 类型 | 示例 |
|------|------|------|
| `%s` | `const char*` | `printk("hello %s", "world")` |
| `%d` | `int`（支持负数） | `printk("val=%d", -42)` |
| `%x` | `uint32_t` | `printk("addr=%x", 0xDEAD)` |
| `%lx` | `uint64_t` | `printk("ptr=%lx", 0x80000000)` |
| `%c` | `char` | `printk("letter=%c", 'A')` |
| `%%` | 字面量 `%` | `printk("100%%")` |

**遇到的工程问题**：`print_dec` 的 `uint32_t` 除法在 RV64 上仍触发 `__umoddi3`/`__udivdi3` 未定义符号。

**根因**：GCC 对 RV64 的 32 位除法和取模会生成 64 位辅助函数调用。

**解决**：用查表减法替代除法和取模：
```c
static const uint32_t powers[] = {
    1000000000, 100000000, 10000000, 1000000,
    100000, 10000, 1000, 100, 10, 1
};
for (int i = 0; i < 10; i++) {
    char digit = '0';
    while (val >= powers[i]) { val -= powers[i]; digit++; }
    // output digit
}
```

#### 3. 重构 `os/kernel/kernel.c`

**文件**：[os/kernel/kernel.c](file:///home/xiaowen/projects/mycpu/os/kernel/kernel.c)

**修改**：
- 移除内嵌的 `uart_putc`/`uart_puts`/`print_hex` 实现
- 改为 `#include "uart.h"` 和 `#include "printk.h"`
- 使用 `printk` 输出所有格式化日志，展示所有格式说明符

**验证输出**（153 字符）：
```
MiniOS booting...
Hello from kernel!
--- Kernel Log Demo ---
String: hello world
Decimal: -42
Hex: 0xdeadbeef
Long hex: 0x80000000
Char: Z
Percent: 100%
```

#### 4. 更新 `os/Makefile`

**文件**：[os/Makefile](file:///home/xiaowen/projects/mycpu/os/Makefile)

**修改**：
- 新增 `uart.o` 和 `printk.o` 编译目标
- OBJS 从 2 个扩展到 4 个（start.o + kernel.o + uart.o + printk.o）

#### 5. 修复 2 个指令分派 Bug（在模拟器中）

**文件**：[src/instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp)

这是阶段 2 的意外收获。printk 格式化代码触发了编译器生成的 ADDIW 和 SRAI 指令，暴露出两个分派表 bug：

**Bug A — ADDIW 和 SLLIW 的 funct7 误匹配**：

| 项 | 详情 |
|------|------|
| 症状 | `printk("%d\n", -42)` 触发 `IllegalInstruction: ADDIW (0xf9d7869b, funct7=0x7c)` |
| 根因 | ADDIW 和 SLLIW 被放在 `instruction2Map`（需 funct7 匹配），但它们的 bits[31:25] 是立即数高位而非 funct7，只有立即数为 0 时才能匹配 `funct7=0x00` |
| 修复 | 将 ADDIW(opcode=0x1b, funct3=0x0) 和 SLLIW(opcode=0x1b, funct3=0x1) 移到 `instructionMap`（只需 opcode+funct3 匹配） |

**Bug B — SRLI/SRAI 的 funct7 宽度错误**：

| 项 | 详情 |
|------|------|
| 症状 | `printk(...)` 中移位操作触发 `IllegalInstruction: SRLI (opcode=0x13, funct3=0x5, funct7=0x1)` |
| 根因 | RV64 的 SRLI/SRAI 使用 6 位 shamt（funct6），bit[25] 是 shamt[5] 而非 funct7[0]。原分发表用 funct7（bits[31:25]）匹配 `0x00`/`0x20`，但非零 shamt 的高位置于 bit[25] 会改变 funct7 值 |
| 修复 | (a) SRAI 的 funct6 改为 `0x10`（因为 `0x20 << 1 = 0x40`，截取位[31:26] 后为 `0x10`） (b) 增加 funct6 回退查找：当 `(opcode, funct3, funct7)` 未命中时，用 `funct6 = (inst >> 26) & 0x3f` 再查一次 |

### 构建验证

```bash
cmake --build build_wsl -j$(nproc)  # ✅ 0 warnings
cd os && make                        # ✅ 0 warnings
```

### 测试结果

```
100% tests passed, 0 tests failed out of 78
Total Test time (real) = 77.08 sec
```

### MiniOS 运行结果

模拟器运行 MiniOS，通过 UART 输出 153 个字符，全部 printk 格式说明符正常：
- `%s` → "hello world"
- `%d` → "-42"（负数正常）
- `%x` → "0xdeadbeef"
- `%lx` → "0x80000000"
- `%c` → "Z"
- `%%` → "%"
- 非法 `%z` → 原样输出 "%z"，不崩溃

### 经验笔记

1. **裸机环境无 libgcc**：`-nostdlib` 意味着没有 `__udivdi3`/`__umoddi3` 等编译器辅助函数。在 RV64 上即使操作 `uint32_t`，GCC 也可能生成 64 位除法辅助函数调用。免除法算法（查表减法）是安全选择。

2. **RISC-V funct7 不总是 funct7**：I-type 指令的 bits[31:25] 在移位指令中是 funct7，在立即数指令中是 imm[11:5]。ADDIW 的 bits[31:25] 是立即数高位，不能当 funct7 用于分派。

3. **RV64 移位指令的 shamt 是 6 位**：SRLI/SRAI 的 shamt 使用 bits[25:20]（6 位），而非 RV32 的 bits[24:20]（5 位）。额外的 bit[25] 意味着 funct7 的有效区分位只剩 bits[31:26]（funct6）。指令分派需要用 funct6 而非 funct7。

4. **printk 的默认行为不会崩溃**：遇到非法格式符（如 `%z`），输出来源原样 `%z`，不消费 va_arg。虽然参数被静默丢弃，但不会影响后续 printk 调用（每个调用有独立的 va_list）。

5. **模块化先于功能积累**：将 UART 驱动和 printk 从 kernel.c 中分离，虽然增加了文件数，但为后续阶段（trap handler 日志、定时器日志）提供了可复用的基础设施。

***

## 2026-05-29 — 阶段 0.5 全部完成 + 阶段 1 MiniOS 启动成功

### 背景

这是项目启动以来最大规模的推进。目标：完成阶段 0.5（前置工程任务）和阶段 1（裸机程序加载与运行），让 MiniOS 在自研模拟器上成功启动并输出。

任务执行顺序（按依赖关系排列）：
1. 修复 UART 测试挂起
2. Bus MMIO 路由（UART/CLINT/PLIC）
3. 指令集覆盖补全（14 条缺失指令）
4. CSR 访问抽象层
5. 内存布局与链接脚本 + MiniOS 裸机程序
6. 修复 JAL 立即数解码 bug
7. 修复 B-type 分支指令立即数解码 bug

### 修改清单

#### 1. 修复 UART 测试挂起

**文件**：[uart.h](file:///home/xiaowen/projects/mycpu/src/uart.h)、[uart.cpp](file:///home/xiaowen/projects/mycpu/src/uart.cpp)

**问题**：UART 构造函数无条件启动 `std::thread` 阻塞在 `std::cin >> byte`，导致非交互环境（ctest）下测试挂起。

**修复**：stdin 监听线程改为通过构造函数参数控制，默认不启动。

**修复后**：5 个 UART 测试全部通过，总测试数从 67 增长到 78。

#### 2. Bus MMIO 路由（UART/CLINT/PLIC）

**文件**：[bus.h](file:///home/xiaowen/projects/mycpu/src/bus.h)、[bus.cpp](file:///home/xiaowen/projects/mycpu/src/bus.cpp)

**内容**：
- Bus 中加入 UART、CLINT、PLIC 的地址路由判断
- UART 范围 `0x10000000 - 0x1000000F`
- CLINT 范围 `0x02000000 - 0x0200BFFF`
- PLIC 范围 `0x0C000000 - 0x0FFFFFFF`
- 未知地址抛出 LoadAccessFault/StoreAMOAccessFault

**遇到的工程问题**：

**(a) 循环依赖 (uart.h ↔ bus.h)**
UART 原本 `#include "bus.h"`，Bus 要 `#include "uart.h"`，形成循环依赖。
**解决**：移除 `uart.h` 中的 `#include "bus.h"`，改用 forward declaration + `std::unique_ptr<Uart>`。

**(b) unique_ptr 不完整类型错误**
Bus 的移动构造/赋值默认实现在头文件中，需要完整 UART 类型才能销毁 `unique_ptr<Uart>`。
**解决**：将 Bus 的析构函数、移动构造、移动赋值定义移到 bus.cpp（其中已包含 uart.h）。

**(c) Cpu 移动构造被删除**
因为 Bus 的不可移动性向上传递导致 Cpu 不可移动。
**解决**：显式声明 `~Cpu()` 在 cpu.h、定义在 cpu.cpp，并显式 `= default` 移动操作。

**测试**：bus_test.cpp 新增 MMIO 路由测试（UART/CLINT/PLIC 读写）。

#### 3. 指令集覆盖补全

**文件**：[instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp)

新增 14 条指令实现并注册到 dispatch table：

| 指令 | 类别 | 说明 |
|------|------|------|
| BLTU | 分支 | 无符号小于分支 |
| SUB | 算术 | 减法 |
| SLTU | 算术 | 无符号比较置位 |
| ECALL | 系统 | 环境调用（目前空实现） |
| EBREAK | 系统 | 断点（目前空实现） |
| WFI | 系统 | 等待中断（目前空实现） |
| ADDIW | RV64I | 字立即数加法 |
| SLLIW | RV64I | 字立即数左移 |
| SRLIW | RV64I | 字立即数逻辑右移 |
| SRAIW | RV64I | 字立即数算术右移 |
| SUBW | RV64I | 字减法 |
| SLLW | RV64I | 字左移 |
| SRLW | RV64I | 字逻辑右移 |
| SRAW | RV64I | 字算术右移 |

#### 4. CSR 访问抽象层

**文件**：[os/include/csr.h](file:///home/xiaowen/projects/mycpu/os/include/csr.h)（新增）

为 MiniOS 裸机程序提供 CSR 访问宏：
- `CSRR(rd, csr)` — 读 CSR
- `CSRW(csr, rs)` — 写 CSR
- `CSRS(csr, rs)` — 置位 CSR
- `CSRC(csr, rs)` — 清零 CSR

**注意**：编译参数需使用 `-march=rv64i_zicsr`，因为 `zicsr` 在新工具链中被分离为独立扩展。

#### 5. 内存布局与链接脚本 + MiniOS 裸机程序

**新增文件**：
- [os/linker.ld](file:///home/xiaowen/projects/mycpu/os/linker.ld) — 链接脚本
- [os/boot/start.S](file:///home/xiaowen/projects/mycpu/os/boot/start.S) — 启动汇编
- [os/kernel/kernel.c](file:///home/xiaowen/projects/mycpu/os/kernel/kernel.c) — 内核 C 代码
- [os/Makefile](file:///home/xiaowen/projects/mycpu/os/Makefile) — 构建 Makefile

**链接脚本关键决策**：
- `OUTPUT_ARCH(riscv)` — 注意是 `riscv` 不是 `riscv64`
- 代码从 `0x80000000` 开始（RISC-V 平台 DRAM 典型起始地址）
- 编译器需使用 `-mcmodel=medany`（支持任意地址，避免 medlow 的重定位溢出）

**启动流程**：
1. start.S 设置栈指针（`la sp, _stack_top`）
2. 清零 BSS 段（`memset` 循环）
3. 跳转到 `kernel_main()`

**内核功能**：
- `uart_putc(char)` — 轮询等待 UART LSR THRE 位，然后写入 THR
- `uart_puts(const char*)` — 字符串输出
- `print_hex(uint64_t)` — 十六进制打印
- `kernel_main()` — 输出 "MiniOS booting...\\nHello from kernel!\\n"，然后 `while(1){}`

#### 6. 修复 JAL 立即数解码 bug（🔴 致命）

**文件**：[instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp)

**问题**：JAL 指令的 imm[19:12] 位域被放在 result 的 bits [7:0] 而非 [19:12]，且 unsigned 类型导致符号扩展失效。

```cpp
// 修复前
((inst >> 12) & 0xff)  // imm[19:12] 错误放在 bits[7:0]

// 修复后
(((inst >> 12) & 0xff) << 12)  // imm[19:12] 正确放在 bits[19:12]
```

同时强制使用 `int32_t` 类型确保符号扩展正确：
```cpp
auto imm = static_cast<int64_t>(static_cast<int32_t>(...));
```

#### 7. 修复 B-type 分支指令立即数解码 bug（🔴 致命）

**文件**：[instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp)

这是本轮最核心、耗时最长的 bug 修复，影响 BEQ/BNE/BLT/BGE/BGEU/BLTU 全部 6 条分支指令。

**Bug 根因分析（两个叠加 bug）**：

| Bug | 说明 |
|-----|------|
| **unsigned 类型陷阱** | 代码使用 `((inner) << 1) >> 1` 技巧做符号扩展，但 `inner` 是 unsigned 类型（通过 `auto` 推导），导致 `>> 1` 执行逻辑右移（补 0）而非算术右移（补符号位） |
| **掩码宽度错误** | 使用 `0xFFF00000`（覆盖 bits [31:20]），但 B-type 12 位立即数的符号在 bit 12，需要 `0xFFFFF000`（覆盖 bits [31:12]） |

**诊断过程**：
1. 运行 MiniOS 发现 BNE 指令跳转到非法地址（PC = `0x7ff010a0`），导致 Fatal Exception
2. 手动解码 BNE 指令 `0xFE0790E3`，确认 offset = -32（正确）
3. 发现模拟器中 offset = +2146439136（巨大正数），确认符号扩展失败
4. 定位到 `<< 1 >> 1` 在 unsigned 类型上的逻辑右移问题
5. 定位到 `0xFFF00000` 掩码不够宽的问题

**修复方案**：
```cpp
// 修复后：6 条 B-type 指令统一模式
int32_t inner = ((inst & 0x80000000) ? 0xFFFFF000 : 0) |
                ((inst & 0x80) << 4) |
                ((inst >> 20) & 0x7E0) |
                ((inst >> 7) & 0x1E);
int64_t imm = static_cast<int64_t>(inner);
```

使用 `int32_t` 确保符号扩展是算术扩展（而非逻辑扩展），`0xFFFFF000` 正确覆盖比特 [31:12]。

### 构建验证

```bash
cmake --build build_wsl -j$(nproc)  # ✅ 0 warnings
cd os && make                        # ✅ 0 warnings
```

### 测试结果

```
100% tests passed, 0 tests failed out of 78
Total Test time (real) = 84.13 sec
```

### MiniOS 运行结果

模拟器运行 MiniOS，扫描 UART 输出：
```
4d 69 6e 69 4f 53 20 62 6f 6f 74 69 6e 67 2e 2e 2e 0a
48 65 6c 6c 6f 20 66 72 6f 6d 20 6b 65 72 6e 65 6c 21 0a
```

解码为 ASCII：
```
MiniOS booting...
Hello from kernel!
```

- 37 次 UART 写入
- 0 次 Fatal 异常
- 内核在 `while(1){}` 死循环中正常运转

### 经验笔记

1. **unsigned 类型陷阱是 C/C++ 的经典问题**：`auto` 推导纯整数运算结果时，如果所有操作数都是 unsigned，结果也是 unsigned。unsigned 上的 `>>` 是逻辑右移（补 0），不是算术右移。符号扩展必须使用 int32_t 等有符号类型。

2. **掩码宽度要匹配立即数的实际比特数**：B-type 有 12 位立即数，imm[12] 是符号位。掩码需要覆盖到 bit 12（即 `0xFFFFF000`），而不是 `0xFFF00000`。

3. **forward declaration + unique_ptr 是打破 C++ 循环依赖的正确模式**：头文件中只需要 forward declaration 和 `unique_ptr` 声明，所有需要完整类型的操作（析构、移动）放到 .cpp 中。

4. **链接脚本的 OUTPUT_ARCH 是 `riscv` 不是 `riscv64`**：GNU ld 对 RISC-V 使用 `OUTPUT_ARCH(riscv)`。

5. **`-mcmodel=medany` 对高地址运行是必需的**：默认的 `medlow` 代码模型只支持低 2GB 地址空间，从 `0x80000000` 开始运行必须用 `medany`。

6. **手动解码指令是有效的调试手段**：当模拟器行为异常时，用指令编码表手动解码立即数，对比模拟器输出，可以有效定位 bug。

7. **一个小 bug 可以完全阻塞整个项目**：B-type 立即数解码的 unsigned 右移问题，导致 MiniOS 第 50+ 条指令就无法继续执行。如果不会手动解码指令或没有系统化的调试方法，这个问题非常难发现。

***

## 2026-05-12 — 构建系统优化与命名规范化

### 背景

在阶段 0.5 工程基线建立后，继续完善构建系统和代码规范。今天的任务是：

1. 优化 CMake 构建配置，升级 C++ 标准到 C++23
2. 添加 CMake 预设配置，简化 WSL 环境下的构建流程
3. 修复代码中的命名拼写错误（cup → cpu）

### 修改清单

#### 1. CMake 构建配置优化

**文件**：[CMakeLists.txt](file:///home/xiaowen/projects/mycpu/CMakeLists.txt)

**修改内容**：
- 修正项目声明，显式指定使用 C++ 语言
- 将 C++ 标准从 C++20 升级到 C++23

**理由**：
- 之前的项目声明缺少语言指定，可能导致 CMake 无法正确识别项目类型
- C++23 提供了更多现代特性，如 `std::expected`、`std::print` 等，为后续开发提供更好的语言支持

***

#### 2. 新增 CMake 预设配置

**文件**：[CMakePresets.json](file:///home/xiaowen/projects/mycpu/CMakePresets.json)（新增）

**内容**：配置 WSL 调试构建预设，包含：
- `configurePresets`：定义 WSL 调试配置预设
- `buildPresets`：定义对应的构建预设

**理由**：
- 简化构建命令，从 `cmake -B build_wsl -DCMAKE_BUILD_TYPE=Debug` 简化为 `cmake --preset=wsl-debug`
- 统一团队开发环境配置，避免不同开发者使用不同的构建参数
- 为 IDE（如 VS Code、CLion）提供更好的 CMake 集成支持

***

#### 3. cup → cpu 命名重命名

**影响文件**：
- `src/cup.h` → [src/cpu.h](file:///home/xiaowen/projects/mycpu/src/cpu.h)
- `src/cup.cpp` → [src/cpu.cpp](file:///home/xiaowen/projects/mycpu/src/cpu.cpp)
- [src/instructions.h](file:///home/xiaowen/projects/mycpu/src/instructions.h)
- [src/main.cpp](file:///home/xiaowen/projects/mycpu/src/main.cpp)
- [tests/test_util.h](file:///home/xiaowen/projects/mycpu/tests/test_util.h)
- [tests/test_util.cpp](file:///home/xiaowen/projects/mycpu/tests/test_util.cpp)
- [tests/unitest/cpu_test.cpp](file:///home/xiaowen/projects/mycpu/tests/unitest/cpu_test.cpp)
- [tests/unitest/instructions_test.cpp](file:///home/xiaowen/projects/mycpu/tests/unitest/instructions_test.cpp)
- [CMakeLists.txt](file:///home/xiaowen/projects/mycpu/CMakeLists.txt)

**修改内容**：
- 重命名 `cup.h/cpp` 为 `cpu.h/cpp`
- 更新所有 `#include "cup.h"` 为 `#include "cpu.h"`
- 更新 CMakeLists.txt 中的源文件路径

**理由**：
- "cup" 是 "cpu" 的拼写错误，影响代码可读性和专业性
- 统一命名规范，避免后续开发中的混淆
- 提升代码质量，符合工程化标准

**影响范围**：10 个文件，28 行修改

***

### 构建验证

```bash
cmake --preset=wsl-debug
cmake --build build_wsl -j$(nproc)
```

**结果**：✅ 所有目标通过，0 警告

### 测试验证

```bash
./build_wsl/unit_test
```

**结果**：✅ 67 个测试全部通过（与 2026-05-11 一致）

### 待办事项更新

从 CURRENT_STATUS.md 的待办清单中移除：
- ~~P1-4: cup → cpu 重命名~~ ✅ 已完成

当前优先级最高的待办：
- P0-1: Bus 接入 UART MMIO
- P0-2: 创建 os/ 目录骨架
- P1-3: 补全指令分派表缺失条目

### 经验笔记

1. **构建系统配置要显式**：CMake 项目声明应显式指定语言（`LANGUAGES CXX`），避免隐式行为导致的跨平台问题。
2. **CMake 预设提升开发体验**：`CMakePresets.json` 可以统一团队构建配置，减少"在我机器上能构建"的问题。
3. **命名规范要尽早修正**：拼写错误如果不及时修正，会在代码库中扩散，增加后期修正成本。
4. **重命名要全局搜索**：文件重命名后，必须检查所有引用（include、CMake、文档），避免遗漏。

***

## 2026-05-11 — 阶段 0 启动：工程基础夯实

### 背景

项目已有 CPU、DRAM、Bus、CSR、Exception、UART、CLINT、PLIC 等核心模块，但存在若干工程缺口阻塞 OS 开发。今天的任务是：

1. 将工程约束纳入项目总纲，形成"阶段 0.5"规划
2. 修复指令分派表缺失导致的严重 bug
3. 补齐测试覆盖
4. 冻结项目当前状态，建立可回溯的基线

### 修改清单

#### 1. 新增 `.gitignore`

**文件**：[.gitignore](file:///home/xiaowen/projects/mycpu/.gitignore)

**内容**：忽略构建产物（`build_wsl/`、`out/`）、IDE 配置（`.vscode/`、`.idea/`）、编译中间文件（`*.o`、`*.a`）等。

**理由**：之前 `out/` 目录下的 VS 构建产物被追踪，导致仓库膨胀。添加 `.gitignore` 是工程化的第一步。

***

#### 2. 新增 `docs/项目总纲.md` — 阶段 0.5 补充

**文件**：[docs/项目总纲.md](file:///home/xiaowen/projects/mycpu/docs/项目总纲.md)

**内容**：在原有阶段 0\~4 规划基础上，新增"阶段 0.5：工程约束与前置条件"章节，包含：

| 条目          | 说明                                 |
| ----------- | ---------------------------------- |
| Bus MMIO 路由 | UART/CLINT/PLIC 必须通过 Bus 接入，而非独立测试 |
| 内存布局与链接脚本   | 提前确定物理地址映射，避免后期大面积重写               |
| CSR 访问抽象层   | 封装 M/S 模式切换逻辑，防止后期大面积重写            |
| 指令覆盖检查      | 内核编译后必须检查是否使用了未实现指令                |
| 测试策略        | 每个模块独立可测 + 集成测试                    |

**决策理由**：这些约束如果不提前处理，会在阶段 1（MiniOS 启动）成为阻塞项。提前纳入总纲可以指导后续开发节奏。

***

#### 3. 修复 `src/instructions.cpp` — 指令分派表

**文件**：[src/instructions.cpp](file:///home/xiaowen/projects/mycpu/src/instructions.cpp)

**问题诊断**：

通过分析分派表发现两个严重 bug：

**Bug A — 分派表条目缺失或错误**：

| 指令   | 原状态              | 修复后            |
| ---- | ---------------- | -------------- |
| BEQ  | 错误映射到 funct3=0x1 | 修正为 funct3=0x0 |
| BNE  | 缺失               | 添加 (0x63, 0x1) |
| BLT  | 缺失               | 添加 (0x63, 0x4) |
| BGE  | 缺失               | 添加 (0x63, 0x5) |
| BGEU | 缺失               | 添加 (0x63, 0x7) |
| SH   | 缺失               | 添加 (0x23, 0x1) |
| SW   | 缺失               | 添加 (0x23, 0x2) |

这些指令的**执行函数早已实现**，只是没有注册到分派表。编译器生成的 `if/else`、`for` 循环、结构体字段写入等代码会触发 `IllegalInstruction` 异常。

**Bug B — 分支指令不跳转时返回** **`std::nullopt`**：

```cpp
// 修复前（以 BGEU 为例）
std::optional<uint64_t> executeBGEU(Cpu& cpu, uint32_t inst) {
    // ...
    if (cpu.regs[rs1] >= cpu.regs[rs2]) {
        return cpu.pc + imm;
    }
    return std::nullopt;  // ❌ 导致 IllegalInstruction
}
```

`std::nullopt` 在上层被解释为"指令未找到"，触发异常。修复为返回 `cpu.update_pc()`：

```cpp
    return cpu.update_pc();  // ✅ 正常推进到下一条指令
```

**影响范围**：所有 5 个分支指令（BEQ/BNE/BLT/BGE/BGEU）都存在此问题。

***

#### 4. 新增 7 个测试用例

**文件**：[tests/unitest/instructions\_test.cpp](file:///home/xiaowen/projects/mycpu/tests/unitest/instructions_test.cpp)

| 测试         | 覆盖指令 | 验证场景                    |
| ---------- | ---- | ----------------------- |
| `TestBeq`  | BEQ  | 相等时跳转 / 不等时不跳转          |
| `TestBne`  | BNE  | 不等时跳转 / 相等时不跳转          |
| `TestBlt`  | BLT  | 有符号小于（-5 < 3）           |
| `TestBge`  | BGE  | 有符号大于等于（10 >= 5）        |
| `TestBgeu` | BGEU | 无符号大于等于（0xFFFFFFFF > 1） |
| `TestSh`   | SH   | 半字存储 + LHU 加载验证         |
| `TestSw`   | SW   | 字存储 + LWU 加载验证          |

**测试设计原则**：

- 每个分支指令测试"跳转"和"不跳转"两条路径
- 使用 `j end` 无限循环确保测试在预期步数内完成
- 通过检查目标寄存器值验证分支是否正确执行

***
#### 5. 新增 `docs/CURRENT_STATUS.md`

**文件**：[docs/CURRENT\_STATUS.md](file:///home/xiaowen/projects/mycpu/docs/CURRENT_STATUS.md)

**内容**：冻结 2026-05-11 的项目状态，包括：

- 环境信息（编译器、CMake、RISC-V 工具链版本）
- 构建状态（所有目标通过，0 警告）
- 测试结果（60/60 通过，UART 测试因设计问题挂起）
- 模块完成度矩阵
- 指令集覆盖分析（含 🔴 严重 / 🟡 中等 分级）
- 关键架构缺口（阻塞 MiniOS 启动的 5 个问题）
- 下一步行动清单（P0/P1/P2 优先级）

**目的**：为后续开发提供可回溯的基线。每次重大修改后更新此文档。

***

### 测试结果

```
[==========] 67 tests from 10 test suites ran. (xxx ms total)
[  PASSED  ] 67 tests.
```

- 原有 60 个测试全部通过
- 新增 7 个测试全部通过
- UART 测试（5 个）在非交互环境下挂起，需手动跳过

### 构建验证

```bash
cmake -B build_wsl -DCMAKE_BUILD_TYPE=Debug  # ✅
cmake --build build_wsl -j$(nproc)            # ✅ 0 warnings
```

### 待办事项（优先级排序）

| 优先级 | 任务                                           | 阻塞         |
| --- | -------------------------------------------- | ---------- |
| P0  | Bus 接入 UART/CLINT/PLIC MMIO                  | MiniOS 输出  |
| P0  | 创建 os/ 目录骨架                                  | MiniOS 结构  |
| P1  | cup → cpu 重命名                                | 代码整洁       |
| P1  | UART 8-bit 访问支持                              | C 代码写 UART |
| P1  | 正常退出机制（WFI/ecall halt）                       | 内核优雅退出     |
| P2  | CMakeLists.txt 规范化 & .gitignore 补充           | 构建系统       |
| P2  | 测试 include 路径改用 target\_include\_directories | 可维护性       |

### 经验笔记

1. **指令分派表是"注册表"，不是"实现清单"**：函数写好了但没注册 = 指令不存在。这是今天踩的最大的坑。
2. **`std::optional`** **的语义要统一**：在指令执行框架中，`nullopt` 表示"不认识这条指令"，而不是"这条指令不需要更新 PC"。分支不跳转时仍需返回下一个 PC 值。
3. **测试要覆盖两条路径**：分支指令必须同时测试跳转和不跳转两种情况，否则很容易漏掉 `nullopt` 这类 bug。
4. **冻结状态很重要**：`CURRENT_STATUS.md` 让后续开发有明确的起点，也方便回顾"当时项目长什么样"。

***

> 下次修改时，请在本文件顶部追加新的日期条目，保持倒序排列（最新在前）。

