# MiniOS / myCPU 项目当前状态

> 冻结时间：2026-06-09
> 冻结版本：阶段 0.5 + 阶段 1 + 阶段 2 + 阶段 3 + 阶段 4 + 阶段 5 + 阶段 6 + 阶段 7 + 阶段 8 + 阶段 9 + 阶段 10 + 模块一至模块七 全部完成

## 一、环境信息

| 项目 | 版本 / 路径 |
|------|-------------|
| OS | WSL (Linux) |
| 编译器 | GCC 13.3.0 |
| CMake | 4.2.3 |
| C++ 标准 | C++23 |
| RISC-V 工具链 | riscv64-unknown-elf-gcc 13.2.0 (已安装) |
| RISC-V objcopy | riscv64-unknown-elf-objcopy (可用) |
| 测试框架 | Google Test (third_party/googletest) |
| 构建目录 | build_wsl/ |

## 二、构建状态

### 构建命令

```bash
# 模拟器 + 测试
cmake --build build_wsl -j$(nproc)

# MiniOS 内核
cd os && make
```

### 构建结果

| 目标 | 状态 | 编译警告 |
|------|------|----------|
| cemu (模拟器) | ✅ 通过 | 0 |
| unit_test (测试) | ✅ 通过 | 0 |
| common_library | ✅ 通过 | 0 |
| MiniOS (kernel.bin) | ✅ 通过 | 0 |

### 产物

- `build_wsl/cemu` — 模拟器可执行文件
- `build_wsl/unit_test` — 单元测试可执行文件
- `build_wsl/libcommon_library.a` — 公共库
- `os/build/kernel.bin` — MiniOS 裸机二进制
- `os/build/kernel.elf` — MiniOS ELF 文件（含调试符号）

## 三、测试结果（2026-06-04 运行）

### 测试总览

| 类别 | 总数 | 通过 | 失败 | 跳过 |
|------|------|------|------|------|
| RV 指令测试 (RVTests) | 32 | 32 | 0 | 0 |
| CSR 综合测试 (CSRSTest) | 1 | 1 | 0 | 0 |
| DRAM 测试 (DramTest) | 4 | 4 | 0 | 0 |
| Bus 测试 (BusTest) | 4 | 4 | 0 | 0 |
| CPU 测试 (CpuTest) | 4 | 4 | 0 | 0 |
| CSR 单元测试 (CsrTest) | 4 | 4 | 0 | 0 |
| 异常测试 (ExceptionTest) | 4 | 4 | 0 | 0 |
| PLIC 测试 (PlicTest) | 13 | 13 | 0 | 0 |
| CLINT 测试 (ClintTest) | 13 | 13 | 0 | 0 |
| UART 测试 (UartTest) | 5 | 5 | 0 | 0 |
| MMU 测试 (MmuTest) | 13 | 13 | 0 | 0 |
| 自定义指令测试 | 2 | 2 | 0 | 0 |
| **合计** | **91** | **91** | **0** | **0** |

### MiniOS 运行验证

```bash
timeout 6 ./build_wsl/cemu os/build/kernel.bin > /tmp/cemu_out.log 2>&1
python3 -c "
import re
with open('/tmp/cemu_out.log') as f:
    chars = [chr(int(m.group(1),16)) for line in f if (m := re.search(r'storing value ([0-9a-f]+) at UART', line))]
print(''.join(chars))
"
```

**输出**（阶段 10 Sv39 分页模式运行，模块一主动停机）：
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
--- Phase 5: Memory Allocator Test ---
Free pages: 32701
--- Phase 10: Virtual Memory (Sv39) ---
Sv39 page table setup complete (identity map 128MB DRAM)
Alloc p1: 0x80006000
Alloc p2: 0x80007000
Alloc p3: 0x80008000
Free pages after alloc: 32695
p1 data: ABCDEFGHIJKLMNOP
Free pages after kfree(p2): 32696
Alloc p4: 0x80007000 (should == p2: 0x80007000)
Free pages after cleanup: 32698
Memory allocator test passed!
--- Phase 3: ECALL Trap Test ---
Triggering ECALL to test trap handler...
=== TRAP ===
scause: 0x9
sepc:   0x800003dc
stval:  0x73
Type: Exception (0x9)
  -> Environment call from S-mode
Unknown syscall number: 0
=== TRAP END ===
Returned from trap handler!
Trap round-trip successful!
--- Phase 8: System Call Test ---
...
System call test passed!

--- Phase 11: User Mode (U-Mode) Test ---
User mode initialized (text=0x80008000 stack=0x80009000 code=82 bytes)
Entering user mode...
=== TRAP ===
scause: 0x8
sepc:   0x10034
stval:  0x73
Type: Exception (0x8)
  -> Environment call from U-mode
Hello from user!
=== TRAP END ===
=== TRAP ===
scause: 0x8
sepc:   0x10040
stval:  0x73
Type: Exception (0x8)
  -> Environment call from U-mode

--- System Exit (code=0) ---
```

- ECALL from S-mode 触发 trap → handler 解码 scause=0x9 → sepc+4 → sret 返回 → 继续执行
- 阶段 8：通过 ecall + a7(系统调用号) + a0-a2(参数) 实现 sys_write 和 sys_exit，syscall_dispatch 读写 trap frame 完成参数传递和返回值写入
- 定时器中断经 mideleg[5] 委托到 S 模式（cause=5, bit63 置位 = 0x8000000000000005），由 sret 返回
- 内存分配器：内核区域约 102KB，堆区从 0x80003000 开始，剩余约 32701 页可分配。bump+free_list 混合算法，O(1) 初始化,分配/释放均页对齐
- 协作式调度（阶段6）：3 个任务（idle + task_a + task_b）按 Idle → A → B 顺序轮转，yield() 主动让出 CPU
- 抢占式调度（阶段7）：定时器中断驱动 sched_tick(tf)，通过修改 trap frame 中 callee-saved 寄存器和 sepc 实现任务切换。3 个任务（idle + task_a + task_b）按 Idle → A → B 顺序轮转，每个周期约 5 次 printk 后切换，count 单调递增无错，上下文切换正确
- S 模式内核（阶段9）：M 模式启动后通过 medeleg/mideleg 委托异常/中断给 S 模式，mret 切换到 S 模式运行 kernel_main。ECALL from S/U-mode 委托到 S 模式 trap handler，定时器中断经 mideleg[5] 委托为 S 模式定时器中断（cause=5），sret 恢复 SPP→mode
- Sv39 虚拟内存（阶段10）：kalloc 分配 3 页（12KB）构建 Sv39 页表，根页表 vpn2=2 映射 DRAM 128MB（2MB 大页 × 64），vpn2=0 映射 MMIO（CLINT/PLIC/UART，2MB 大页）。开启 SATP 后所有已有功能（ECALL、系统调用、抢占式调度）在模拟 MMU 下正常运行。模拟器 MMU 支持 Bare/Sv39 模式、三级地址翻译、4KB/2MB 页、A/D 位自动设置、权限检查（R/W/X）和 3 种页异常（Instruction/Load/Store PageFault）

## 四、模块完成度

### 已完成的模块

| 模块 | 文件 | 状态 | 说明 |
|------|------|------|------|
| DRAM | dram.h/cpp | ✅ 完成 | 128MB 物理内存，8/16/32/64 位读写，小端序 |
| CSR | csr.h/cpp | ✅ 完成 | 4096 槽 CSR 数组，M/S 模式寄存器映射，medeleg/mideleg 委托逻辑 |
| Exception | exception.h/cpp | ✅ 完成 | 14 种 RISC-V 异常类型，isFatal 判断，格式化输出 |
| PLIC | plic.h/cpp | ✅ 完成 | 独立可测，pending/senable/spriority/sclaim 寄存器读写 |
| CLINT | clint.h/cpp | ✅ 完成 | 独立可测，mtime/mtimecmp 寄存器读写 |
| UART | uart.h/cpp | ✅ 完成 | NS16550A，stdin/stdout 字符 I/O，stdin 线程可控启动 |
| CPU | cpu.h/cpp | ✅ 完成 | PC、32 通用寄存器、M/S/U 模式、fetch/execute/handle_exception |
| Bus MMIO 路由 | bus.h/cpp | ✅ 完成 | UART/CLINT/PLIC/DRAM 全部路由，循环依赖已解决 |
| MMU | mmu.h/cpp | ✅ 完成（阶段 10） | Sv39 地址翻译，三级页表遍历，4KB/2MB 页支持，权限检查，Bare/Sv39 模式 |
| 指令集 | instructions.h/cpp | ✅ 完成 | 约 50 条 RV64I 指令，dispatch table 完整 |
| MiniOS 裸机程序 | os/boot/, os/kernel/ | ✅ 完成 | 启动汇编 + C 内核 + 链接脚本 + Makefile |
| MiniOS UART 驱动 | os/kernel/uart.h/c | ✅ 完成 | uart_init/uart_putc/uart_puts 分离为独立模块 |
| MiniOS 内核日志 | os/kernel/printk.h/c | ✅ 完成 | 支持 %s/%d/%x/%lx/%c/%%，免除法实现避免依赖 libgcc |
| CSR 抽象层 | os/include/csr.h | ✅ 完成 | CSRR/CSRW/CSRS/CSRC 宏封装，含 trap 别名宏（TRAP_VEC/EPC/CAUSE/TVAL/STATUS） |
| Trap 入口汇编 | os/kernel/trap.S | ✅ 完成（阶段 3，阶段 7 增强，阶段 9 适配） | 保存全部 32 个寄存器到 trap frame (256B)，将 sp 作为 a0 传入 trap_handler，恢复后 sret |
| Trap handler | os/kernel/trap.h/c | ✅ 完成（阶段 3，阶段 7 增强，阶段 9 适配） | trap_handler(tf) 接收 trap frame 指针，定时器中断调用 sched_tick(tf)，scause/sepc/stval 替代 mcause/mepc/mtval，trap_set_silent() 静默模式 |
| 定时器驱动 | os/kernel/timer.h/c | ✅ 完成（阶段 4，阶段 9 适配） | CLINT MMIO 读写，mtimecmp 设置，sie.STIE 使能（替代 mie.MTIE），timer_handle 精简为纯 set + count |
| CPU 中断检测 | cpu.h/cpp | ✅ 完成（阶段 4，阶段 9 增强） | check_pending_interrupts() 支持 S 模式委托中断检测 + handle_interrupt() 支持 S 模式中断处理 |
| 物理内存分配器 | os/kernel/mem.h/c | ✅ 完成（阶段 5） | bump allocator + 空闲链表混合，kalloc/kfree/mem_free_pages |
| 任务管理 | os/kernel/task.h/c | ✅ 完成（阶段 6，阶段 7 增强） | task_struct + context 帧，yield() 协作式调度，sched_tick(tf) 抢占式调度入口 |
| 上下文切换 | os/kernel/switch.S | ✅ 完成（阶段 6） | switch_to 汇编，保存/恢复 callee-saved 寄存器（ra/sp/s0-s11）；抢占式用 trap frame 路径，不调用 switch_to |
| 系统调用 | os/kernel/syscall.h/c | ✅ 完成（阶段 8） | syscall_dispatch(tf)，SYS_WRITE/SYS_EXIT，trap frame 中读写 a0/a7，ecall 往返验证通过 |
| 抢占式调度 | task.c (sched_tick) | ✅ 完成（阶段 7） | 定时器中断 → trap_handler → sched_tick(tf) 修改 trap frame + sepc → sret 跳转新任务 |
| M→S 模式启动 | os/boot/start.S | ✅ 完成（阶段 9） | M 模式配置 medeleg/mideleg/stvec → mret 切换到 S 模式 → kernel_main |
| S 模式 trap 处理 | os/kernel/trap.S/c | ✅ 完成（阶段 9） | sret 替代 mret，scause/sepc/stval 替代 mcause/mepc/mtval，中断/异常 cause 编码适配 S 模式 |
| Timer 中断委托 | main.cpp + timer.c | ✅ 完成（阶段 9） | mideleg[5] 委托定时器中断到 S 模式（cause=5），MTIP→STIP 映射，sie.STIE 使能 |
| SIP/MTIP 映射 | csr.cpp | ✅ 完成（阶段 9） | SIP load 正确映射委托中断位（SSIP←MSIP, STIP←MTIP, SEIP←MEIP），SIP store bug 修复 |
| MiniOS 内核 VM | os/kernel/vm.h/c | ✅ 完成（阶段 10） | Sv39 页表初始化，身份映射 DRAM 128MB + MMIO（CLINT/PLIC/UART） + TEST_FINISH，2MB 大页 |
| 主动停机（Shutdown） | bus.h/cpp + main.cpp + syscall.c | ✅ 完成（模块一） | TEST_FINISH(0x100000) MMIO 设备，内核写入触发 halted 标志，主循环检测后退出 |
| U 模式切换 | os/kernel/user.h/c + user_entry.S | ✅ 完成（模块二） | S 模式内核通过 sret 切换到 U 模式入口 0x10000，构建用户页表映射代码页/栈页/TEST_FINISH |
| 用户态系统调用 | user_entry.S + trap.c + syscall.c | ✅ 完成（模块三） | U 模式 ecall(scause=8) → S 模式 trap → sys_write(64) 输出 "Hello from user!" → sys_exit(93) 写入 TEST_FINISH 停机 |
| 用户地址空间权限隔离 | mmu.cpp + vm.c + vm.h + bus.h/cpp | ✅ 完成（模块四） | MMU translate 增加 mode 参数，U 位检查阻止用户态访问内核页（U=0），新增 3 个 MMU 测试，91/91 全通过 |
| UART 输入与 sys_read | uart.c/h + syscall.c/h + bus.h/cpp + main.cpp | ✅ 完成（模块五） | 内核端 uart_getc()/uart_has_data()，SYS_READ(63) 支持单字符阻塞读取（遇换行符终止），stdin 监听改用 poll() 非阻塞 |

### 未完成的模块

| 模块 | 状态 |
|------|------|
| 任务状态与 exit/wait 雏形 | ❌ 未开始 |
| RAMFS 最小内存文件系统 | ❌ 未开始 |
| 完整进程管理（ELF loader、fork/exec/wait） | ❌ 未开始 |

## 五、指令集覆盖

所有阶段 0.5 规划内的指令已全部实现并注册到 dispatch table：

| 类别 | 已实现指令 |
|------|-----------|
| 算术立即数 | ADDI, SLTI, SLTIU, XORI, ORI, ANDI |
| 移位立即数 | SLLI, SRLI, SRAI |
| 算术 | ADD, SUB, SLT, SLTU, XOR, OR, AND |
| 移位 | SLL, SRL, SRA |
| 加载 | LB, LH, LW, LD, LBU, LHU, LWU |
| 存储 | SB, SH, SW, SD |
| 分支 | BEQ, BNE, BLT, BGE, BLTU, BGEU |
| 跳转 | JAL, JALR |
| 高位立即数 | LUI, AUIPC |
| CSR | CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI |
| 特权 | SRET, MRET, SFENCE.VMA |
| Fence | FENCE |
| RV64I | ADDIW, SLLIW, SRLIW, SRAIW, ADDW, SUBW, SLLW, SRLW, SRAW |
| 系统 | ECALL, EBREAK, WFI |

**状态**：✅ 无缺失，dispatch table 完整。

## 六、设计决策记录

### Decision 1: 循环依赖解决

**决策**: UART ↔ Bus 循环依赖用 forward declaration + `std::unique_ptr<Uart>` 打破  
**理由**: Bus 拥有 UART，UART 不需要知道 Bus 的存在（它只暴露 load/store 接口）  
**后果**: Bus 的析构函数和移动操作必须在 bus.cpp 中定义（需要完整 UART 类型）

### Decision 2: B-type 立即数解码

**决策**: 使用 `int32_t` 中间类型 + `static_cast<int64_t>` 做符号扩展，而非 `<< 1 >> 1` 技巧  
**理由**: `<< 1 >> 1` 在 unsigned 类型上执行逻辑右移（补 0），不进行符号扩展。B-type 12 位立即数符号位在 bit 12，需要 `0xFFFFF000` 掩码（不是 `0xFFF00000`）  
**后果**: 所有 6 条 B-type 分支指令（BEQ/BNE/BLT/BGE/BGEU/BLTU）和 JAL 统一采用此模式

### Decision 3: 链接脚本起始地址

**决策**: MiniOS 从 `0x80000000` 开始运行  
**理由**: 这是 RISC-V 平台上 DRAM 的典型起始地址，与 QEMU virt 平台兼容  
**后果**: 编译器需使用 `-mcmodel=medany`（代码模型支持任意地址）

### Decision 4: printk 免除法实现

**决策**: `print_dec` 使用查表减法（预计算 10 的幂，循环减）替代 `%` 和 `/` 运算符  
**理由**: 裸机环境下 `-nostdlib` 不链接 libgcc，64 位除法和取模会触发 `__udivdi3`/`__umoddi3` 未定义符号  
**后果**: 代码稍长但无外部依赖，纯 RV64I 指令即可运行

### Decision 5: ADDIW/SLLIW 分派策略

**决策**: ADDIW 和 SLLIW 从 `instruction2Map`（需要 funct7 匹配）移到 `instructionMap`（只需 opcode+funct3 匹配）  
**理由**: ADDIW 的 bits[31:25] 是立即数高位而非 funct7，ADDIW 没有 funct7 变体，不需要三重匹配  
**后果**: SRLIW/SRAIW 保留在 instruction2Map（它们确实需要 funct7 区分逻辑/算术右移）

### Decision 6: SRLI/SRAI funct6 回退

**决策**: 当 `(opcode, funct3, funct7)` 查不到时，回退用 `funct6 = (inst >> 26) & 0x3f` 再查一次  
**理由**: RV64 的 SRLI/SRAI 使用 6 位移位量，`shamt[5]` 落在 bit[25]（原 funct7 的 bit 0），导致非零移位量无法匹配 `funct7=0x00`/`0x20`  
**后果**: SRAI 的 funct6 值为 `0x10`（不是 `0x20`），因为 `0x20 << 1 = 0x40` 的第 6 位会被 funct6 截断

### Decision 7: Trap 帧寄存器保存顺序与 sp 恢复

**决策**: 在 trap_entry 中先单独保存/恢复 t0，再用 t0 计算原始 sp；恢复时 sp 最后恢复
**理由**: 需要借用 t0 来计算进入 trap 前的 sp（`sp + 256`）。如果恢复寄存器时先恢复 sp（旧版 bug），`ld sp, 16(sp)` 之后 sp 改变，所有后续 `ld` 都从错误地址加载，导致寄存器值混乱进而触发 LoadAccessFault
**后果**: trap.S 的保存顺序包含"先 st0 → 计算 sp → 恢复 t0"的微妙三步；恢复顺序中 sp 必须是最后一条 ld

### Decision 8: mepc+4 推进策略

**决策**: trap handler 在返回前执行 `trap_epc_write(epc + 4)`
**理由**: ECALL 是 4 字节指令，mret 回到 mepc 指向的地址。如果不推进 mepc，mret 会回到同一条 ECALL，形成无限循环（每个 trap 重新执行 ECALL → 再次 trap）
**后果**: 此策略对非 ECALL 类异常（如非法指令、断点）也需要推进 mepc 否则会死循环；未来阶段可能需要按异常类型决定推进量（如压缩指令 2 字节）

### Decision 9: 中断 vs 异常的 mepc 语义分离

**决策**: trap handler 对异常执行 `mepc+4`，对中断不推进 mepc
**理由**: 异常（如 ECALL）的 mepc 指向触发异常的指令本身，需要 +4 跳过；中断在指令边界采样，mepc 指向尚未执行的被中断指令，mret 应直接回到该指令继续执行
**后果**: trap.c 中中断分支必须 `return` 且不调用 `trap_epc_write()`，异常分支继续执行 `mepc+4`

### Decision 10: 中断检测在取指前而非执行后

**决策**: main.cpp 主循环中，`check_pending_interrupts()` 放在 `fetch()` 之前
**理由**: RISC-V 规范定义中断在指令执行完成后采样，即下一条指令取指前。每周期先 tick CLINT 更新 MTIP，然后检查中断，最后正常取指执行
**后果**: `handle_interrupt` 返回后 `continue` 跳过当次 fetch/execute，下一轮循环重新进入中断检查（中断处理完成后 MTIP 已清除，正常取指执行）

### Decision 11: MTIP 由 main.cpp 根据 tick() 结果自动管理

**决策**: 不在 CLINT 内部设置 CSR，而是在 main.cpp 中根据 `clint.tick()` 返回值设置/清除 `MIP.MTIP`
**理由**: 保持 CLINT 模块的独立性（不依赖 CSR），中断挂起位是 CPU 核心的职责，由 main.cpp 作为粘合层连接 CLINT 和 CSR
**后果**: tick() 返回 true 时置位 MTIP，返回 false 时清除 MTIP，确保内核更新 mtimecmp 后自动清除中断挂起

### Decision 12: 混合 bump allocator + 空闲链表

**决策**: 分配器使用 bump allocator（首次分配时推进指针）+ 空闲链表（kfree 回收的页链入链表），kalloc 优先从空闲链表取页
**理由**: 纯空闲链表方案需要在 mem_init() 中遍历 32702 页逐个链入，模拟器逐条指令执行导致初始化极慢（O(n) 遍历）。Bump allocator 初始化只需计算 `(heap_end - bump_ptr) / PAGE_SIZE`（O(1)），kfree 回收的页再链入空闲链表供后续复用
**后果**: 首次分配走 bump（O(1)），释放后重新分配走 free_list（O(1)）。栈保护区（256KB）保证内核栈不被覆盖

### Decision 13: 抢占式调度通过 trap frame 实现

**决策**: `sched_tick(tf)` 通过直接修改 trap frame 中 callee-saved 寄存器和 mepc 来切换任务，而非调用 `switch_to`
**理由**: 抢占式调度发生在中断上下文中，当前任务的完整寄存器状态已由 `trap_entry` 保存到 trap frame 中。`sched_tick` 只需将当前任务 callee-saved 状态提取到 `current->ctx`，将 `next->ctx` 写回 trap frame，陷阱返回后 `trap_entry` 的尾随恢复逻辑自然将新任务的寄存器加载到 CPU
**后果**: 
- sched_tick 不需要调用 switch_to（switch_to 用于协作式 yield 路径）
- 新任务通过修改 mepc 为 `next->ctx.ra` 来指定 mret 的目标地址
- 新创建的任务 mepc 指向 entry 函数，被抢占的任务 mepc 指向被中断的指令
- **重要 Bug 修复**：最初 `sched_tick()` 通过 `__asm__ volatile("mv %0, sp" : "=r"(sp))` 获取 trap frame 基址，但此时 sp 已指向 `sched_tick` 自己的栈帧（因为经历了 trap_entry → trap_handler → sched_tick 多层函数调用），获取的是错误的栈地址。正确做法是：`trap_entry` 在分配 trap frame 后执行 `mv a0, sp`，将 trap frame 基址通过参数链 `trap_entry(a0) → trap_handler(tf) → sched_tick(tf)` 层层传递

### Decision 14: M 模式启动 → mret 切换到 S 模式

**决策**: 内核从 M 模式启动，在 `_start` 中配置 medeleg/mideleg/stvec 后将 MPP 设为 Supervisor，通过 mret 切换到 S 模式运行 `kernel_main`
**理由**: RISC-V 没有"跳转到 S 模式"的指令，唯一途径是 mret 指令。在 mstatus.MPP 中设置目标特权级，在 mepc 中设置目标地址，mret 后 CPU 自动切换特权级并跳转
**委托配置**:
- `medeleg[8:9]`: ECALL from U/S-mode → S-mode（cause=8/9 替代 M-mode cause=10/11）
- `mideleg[5]`: Supervisor timer interrupt → S-mode（cause=5 替代 M-mode cause=7）
**后果**:
- S 模式 trap handler 使用 scause/sepc/stval/sstatus，不再使用 M 模式 CSR
- 定时器中断从 cause=7(MTI) 变为 cause=5(STI)，trap.c 的 switch 分支需对应修改
- `MINIOS_USE_S_MODE` 宏使内核 C 代码通过 `trap_epc_read()` 等抽象层自动使用 S 模式 CSR
- 保留 `trap_entry_m` 作为 M 模式 trap 向量（死循环），处理非预期陷阱

### Decision 15: SIP 委托中断位映射不是简单 AND

**决策**: `csr.load(SIP)` 需按 cause 编号做位映射（SSIP←MSIP, STIP←MTIP, SEIP←MEIP），而非 `MIP & MIDELEG`
**理由**: RISC-V 规范中，M 模式中断 cause N 委托到 S 模式后 cause 为 N-2。对应的 pending 位也需从 MIP bit(2N+3) 映射到 SIP bit(2(N-2)+1)。MTIP(bit7) 和 STIP(bit5) 是不同的 bit，`MIP & MIDELEG` = `(1<<7) & (1<<5)` = 0，定时器中断永远无法被检测到
**后果**: CSR 模块的 SIP load 需要遍历 mideleg 的每个委托位，逐位映射 MIP 对应位到 SIP

### Decision 16: MMU 直连 Dram 而非通过 Bus 进行页表遍历

**决策**: `Mmu::translate()` 内部的页表遍历使用 `dram.load()` 直接读取物理内存中的页表项，而非通过 `Bus::load()`
**理由**: 页表遍历本身是 MMU 的硬件行为，在物理地址空间中进行。Bus 包含 MMIO 路由逻辑（UART/CLINT/PLIC），如果页表遍历经过 Bus，地址路由会产生不必要的开销和逻辑耦合。此外页表基址必须在 DRAM 范围内，通过 Dram 直接访问是最简洁的实现
**后果**: MMU 持有 `Dram&` 引用，页表遍历性能等于直接内存访问

### Decision 17: CPU 所有内存访问统一经过 MMU 翻译

**决策**: `Cpu::fetch()`、`Cpu::load()`、`Cpu::store()` 在访问 Bus 之前先调用 `mmu.translate()` 进行虚拟地址转物理地址
**理由**: 真实 RISC-V 处理器中，MMU 位于 CPU 和总线之间的关键路径上，所有访存请求都必须经过地址翻译。在 SATP.MODE=Bare 时 translate 返回原地址（直通），SATP.MODE=Sv39 时进行三级页表翻译
**后果**: 从调用层面看，Bus 现在接收的都是物理地址，接口语义更清晰。已通过 10 项 MMU 单元测试和 MiniOS 全功能端到端验证

### Decision 18: MMIO 区域也需在页表中映射

**决策**: 内核页表除了身份映射 DRAM 外，还必须在 `vpn2=0` 的 LV1 表中映射 CLINT/PLIC/UART 三个 MMIO 区域
**理由**: 开启 Sv39 分页后 CPU 所有 load/store 都经过虚拟地址翻译。UART（0x10000000）、CLINT（0x02000000）、PLIC（0x0C000000）都不在 DRAM 范围内，如果不为它们建立页表映射，printk、timer_init 等所有外设访问都会触发 Load/Store PageFault，内核立即崩溃
**后果**: 页表从 2 页增加到 3 页（共用根页表，DRAM 和 MMIO 各一个 LV1 表），kalloc 消耗 12KB

## 七、已知问题

| 问题 | 严重性 | 状态 |
|------|--------|------|
| ~~B-type 分支指令符号扩展 bug~~ | 致命 | ✅ 已修复 |
| ~~JAL 立即数解码 bug~~ | 致命 | ✅ 已修复 |
| ~~UART 测试挂起~~ | 中 | ✅ 已修复 |
| ~~指令分派表缺失~~ | 致命 | ✅ 已修复 |
| ~~ADDIW 分派表 funct7 误匹配~~ | 致命 | ✅ 已修复（阶段 2） |
| ~~SRLI/SRAI 分派表 funct7 误匹配~~ | 致命 | ✅ 已修复（阶段 2） |
| ~~中断/异常处理框架缺失~~ | 中 | ✅ 已实现（阶段 3） |
| ~~ECALL/EBREAK 为空实现~~ | 低 | ✅ 已实现（阶段 3） |
| ~~CLINT 定时器中断未接入 CPU~~ | 中 | ✅ 已实现（阶段 4） |
| ~~MiniOS 完成后无法主动退出（仅 timeout 终止）~~ | 低 | ✅ 已修复（模块一 — TEST_FINISH MMIO） |
| WFI 指令为 NOP 实现，无法真正暂停 CPU | 低 | 功能正确但效率低，不影响当前阶段 |
| IDE/LSP 报告 RISC-V 内联汇编寄存器名未知（`a0`/`a1`/`a7`） | 低 | ✅ 已修复 — 宿主 x86 语言服务器不认识 RISC-V 寄存器名，用 `#ifdef __riscv` 包裹内联汇编，`#else` 分支提供无害替代。不影响交叉编译和运行 |
| 抢占式调度 sched_tick 未触发任务切换 | 高 | ✅ 已修复（阶段 9 — SIP/MTIP 委托映射修复） |

## 八、当前文件结构

```text
mycpu/
├── CMakeLists.txt
├── CMakePresets.json
├── .gitignore
├── docs/
│   ├── 项目总纲.md
│   ├── 任务.md
│   ├── CURRENT_STATUS.md    (本文档)
│   └── DEVELOPMENT_LOG.md
├── src/
│   ├── main.cpp             (入口：加载 bin 文件，运行主循环)
│   ├── param.h              (地址常量、CSR 编号、掩码定义)
│   ├── log.h                (日志/彩色输出)
│   ├── cpu.h / cpu.cpp      (CPU 核心：PC、寄存器、fetch/execute 循环)
│   ├── dram.h / dram.cpp    (128MB 物理内存)
│   ├── bus.h / bus.cpp      (总线：MMIO 地址路由到 DRAM/UART/CLINT/PLIC)
│   ├── csr.h / csr.cpp      (控制状态寄存器)
│   ├── exception.h / .cpp   (异常建模)
│   ├── instructions.h / .cpp(指令实现 + dispatch table，约 50 条指令)
│   ├── mmu.h / mmu.cpp       (MMU：Sv39 地址翻译、三级页表遍历、权限检查)
│   ├── uart.h / uart.cpp    (NS16550A UART 模型)
│   ├── clint.h / clint.cpp  (CLINT 定时器模型)
│   └── plic.h / plic.cpp    (PLIC 中断控制器模型)
├── os/
│   ├── Makefile             (交叉编译：riscv64-unknown-elf-gcc)
│   ├── linker.ld            (链接脚本：OUTPUT_ARCH(riscv), 0x80000000)
│   ├── boot/
│   │   └── start.S          (启动汇编：设栈、清零 BSS、配置 medeleg/mideleg/stvec、mret 切换到 S 模式 → kernel_main)
│   ├── kernel/
│   │   ├── kernel.c         (内核入口：printk 演示 + 内存分配器测试 + ECALL trap + 系统调用测试 + 抢占式调度测试 + 用户模式演示)
│   │   ├── uart.h           (UART 驱动头文件)
│   │   ├── uart.c           (UART 驱动：uart_putc/uart_puts)
│   │   ├── printk.h         (内核日志头文件)
│   │   ├── printk.c         (内核日志：%s/%d/%x/%lx/%c/%%，免除法)
│   │   ├── trap.S           (trap 入口汇编：寄存器保存/恢复 + sret + 传 trap frame 指针)
│   │   ├── trap.h           (trap handler 头文件)
│   │   ├── trap.c           (trap handler：接收 trap frame，中断分支调用 sched_tick，trap_silent 模式，S 模式 cause 编码)
│   │   ├── timer.h          (定时器驱动头文件)
│   │   ├── timer.c          (定时器驱动：CLINT MMIO 读写 + mtimecmp 设置，S 模式 STIE)
│   │   ├── mem.h            (物理内存分配器头文件)
│   │   ├── mem.c            (物理内存分配器：bump+free_list 混合，kalloc/kfree)
│   │   ├── task.h           (任务管理头文件：task_struct + context + sched_tick)
│   │   ├── task.c           (任务管理：task_init/create/yield/schedule/sched_tick)
│   │   ├── switch.S         (上下文切换汇编：switch_to 保存/恢复 callee-saved 寄存器)
│   │   ├── syscall.h        (系统调用头文件：SYS_WRITE/SYS_EXIT + syscall_dispatch)
│   │   ├── syscall.c        (系统调用：sys_write/sys_exit + trap frame 中 a0/a7 读写)
│   │   ├── user.h           (用户模式头文件：user_init + enter_user)
│   │   ├── user.c           (用户模式：分配物理页 + 构建用户页表 + 复制用户程序 + sret 切换)
│   │   ├── user_entry.S     (用户程序：sys_write(1,msg,17) + sys_exit(0) 两条 ecall)
│   │   ├── vm.h             (虚拟内存头文件：Sv39 PTE 定义 + vm_init + 全局页表指针导出)
│   │   └── vm.c             (虚拟内存：构建身份映射页表 + 开启 SATP + 暴露全局页表指针)
│   └── include/
│       └── csr.h            (CSR 访问抽象层)
├── tests/
│   ├── test_util.h / .cpp
│   ├── riscv-tests/         (RISC-V 汇编测试用例)
│   └── unitest/             (C++ 单元测试)
│       ├── cpu_test.cpp
│       ├── instructions_test.cpp
│       ├── bus_test.cpp     (含 MMIO 路由测试)
│       ├── dram_test.cpp
│       ├── uart_test.cpp
│       ├── clint_test.cpp
│       ├── plic_test.cpp
│       ├── csr_test.cpp
│       ├── mmu_test.cpp      (MMU 单元测试：Bare/Sv39 地址翻译、权限检查、页异常)
│       └── exception.cpp
└── third_party/
    └── googletest/
```

# 九、下一步行动

| 优先级 | 任务 | 所属阶段 |
|--------|------|----------|
| P1 | MMU 检查 PTE_U 位实现用户态权限隔离 | 模块四 |
| P2 | 代码整洁：B-type 6 条指令提取公共立即数解码函数 | 重构 |
| P2 | UART 输入缓冲 + sys_read 系统调用 | 模块五 |

## 十、本轮关键学习点

1. **unsigned 类型陷阱**：`auto` 推导纯整数运算结果为 unsigned。unsigned 上 `>>` 是逻辑右移（补 0），不会做符号扩展。这是 C/C++ 的经典陷阱。
2. **掩码宽度**：B-type 12 位立即数符号在 bit 12，需要 `0xFFFFF000`（20 位掩码），不是 `0xFFF00000`（12 位掩码）。
3. **MMIO 地址路由**：CPU 用 `load/store` 访问外设寄存器，无需独立 I/O 指令。这是 RISC-V 架构的核心理念。
4. **链接脚本**：裸机程序需要链接脚本定义 .text/.data/.bss 的加载地址和链接地址。
5. **forward declaration + unique_ptr**：打破 C++ 循环依赖的模式。
6. **裸机环境无 libgcc**：`-nostdlib` 下没有 `__udivdi3`/`__umoddi3` 等编译器辅助函数，64 位除法需手动避免或用减法替代。
7. **RISC-V funct7 vs funct6**：RV64 的移位立即数指令（SRLI/SRAI/SRLIW/SRAIW）使用 6 位 shamt，bit[25] 是 shamt[5] 而非 funct7[0]。指令分派必须用 funct6（bits[31:26]）而非 funct7（bits[31:25]）。
8. **I-type 指令的 funct7 是立即数的一部分**：ADDIW 的 bits[31:25] 是 12 位立即数的高 7 位，不能当作 funct7 用于分派。
9. **trap 帧的寄存器保存/恢复顺序至关重要**：sp 必须最后恢复，否则 `ld` 的基地址偏移全部错误。借用临时寄存器（如 t0）计算原始 sp 时，需要先保存该寄存器的原始值。
10. **mret 后必须推进 mepc**：如果不修改 mepc，mret 回到同一指令形成死循环。ECALL 需要 `epc+4`，中断需要 `epc+0`（中断在指令边界采样）。
11. **预处理宏的 `#` 字符串化不展开参数**：`#csr` 直接字符串化参数名，不会先展开宏别名。要用间接宏 `_csr_read(csr) → csr_read(csr)` 的两层模式让别名先展开。
12. **M-mode ECALL 的 cause 是 10（0xa）而非 11**：11 是 `EnvironmentCallFromMMode`，但 ECALL 在 M-mode 执行时 cause 为 10。需要对照 RISC-V 特权规范确认具体编码。
13. **中断 vs 异常的 mepc 语义不同**：异常 mepc 指向触发指令本身需 +4；中断 mepc 指向尚未执行指令不需推进。
14. **mret 自动恢复中断使能**：mret 将 MPIE→MIE，再设 MPIE=1。trap handler 不需要手动重新使能中断，但进入 trap 时硬件已自动清除 MIE。
15. **中断在指令边界采样**：RISC-V 规范要求中断在指令执行完成后、下一条指令取指前检查。主循环中 tick→MTIP→check_interrupts→fetch 的顺序正确实现这一语义。
16. **MTIP 由软件管理**：MTIP 不是 CLINT 自动设置的，需要软件（模拟器 main.cpp）根据 mtime >= mtimecmp 的条件来设置/清除。这让 CLINT 保持为纯时序模型，不依赖 CSR 模块。
17. **Bump allocator + 空闲链表混合策略**：初始化时避免 O(n) 遍历所有页（模拟器执行慢），用 O(1) bump 指针初始化，kfree 回收的页链入 free_list。kalloc 优先从 free_list 取页（回收复用），free_list 为空时走 bump（首次分配）。
18. **`extern char` 声明获取链接脚本符号地址**：裸机 C 代码通过 `extern char _kernel_end;` 和 `&_kernel_end` 获取链接脚本定义的符号地址，这是获取内核结束地址的标准做法。
19. **页对齐的向上/向下取整**：`align_up(addr, 4096)` = `(addr + 4095) & ~4095`，`align_down(addr, 4096)` = `addr & ~4095`。这是内存管理的基础工具。
20. **print_hex 自带前缀导致格式串冗余**：`print_hex()` 内部已输出 `0x`，格式串不应再写 `0x`，否则产生 `0x0x`。
21. **Callee-saved vs Caller-saved 是上下文切换的核心**：switch_to 只保存 callee-saved 寄存器 (ra/sp/s0-s11)；caller-saved (t0-t6/a0-a7) 由编译器在 yield() 函数调用的栈帧中自动保存/恢复。
22. **新任务通过伪造 ra 来"启动"**：`ctx.ra = entry` 使 `switch_to` 的 `ret` 直接跳转到任务入口，这是经典的内核任务初始化技巧。
23. **协作式 vs 抢占式使用不同的上下文切换路径**：协作式 yield → switch_to（C 调用汇编，保存/恢复 callee-saved）；抢占式 sched_tick → 直接修改 trap frame + mepc（利用硬件自动保存的所有寄存器）。
24. **trap frame 布局是汇编与 C 之间的 ABI 契约**：trap.S 按固定偏移保存 32 个寄存器到栈上 256B 区域，trap.c 中的 trap_handler(tf) 和 sched_tick(tf) 通过同样的偏移读写 callee-saved 寄存器。这是组装接口设计的关键。`mv a0, sp` 将 trap frame 基址作为第一个参数传入。
25. **函数调用会使 sp 偏移，不能直接读 sp 获取 trap frame**：`sched_tick` 被 `trap_handler` 调用，`trap_handler` 被 `trap_entry` 调用。每层调用 `sp` 都会递减（分配新栈帧），因此 `sched_tick` 内部直接读 `sp` 得到的是自己的栈帧而非 trap frame。正确方式是在最外层（`trap_entry`）将 trap frame 基址通过参数链传递进来。这是裸机/内核编程中的常见陷阱：汇编与 C 之间的指针传递。
26. **mret 是特权级切换的唯一途径**：RISC-V 没有"跳转到 S 模式"或"跳转到 U 模式"的指令。从高特权级进入低特权级只能通过 xRET 指令：在 mstatus 的 xPP 字段设置目标特权级，在 xEPC 中设置目标地址，执行 mret/sret 后 CPU 自动切换。这是 RISC-V 特权架构的核心机制。
27. **medeleg/mideleg 改变整个 trap 入口 CSR 集合**：委托不仅仅是"转发"陷阱。它让硬件使用 stvec 替代 mtvec、sepc 替代 mepc、scause 替代 mcause——两套完全独立的 CSR 集合。
28. **MIP 和 SIP 的位映射不是简单 AND**：MTIP(bit7) 和 STIP(bit5) 是不同的 bit 位置。`(1<<7) & (1<<5)` = 0，导致委托后的定时器中断永远检测不到。正确做法是按 cause 编号映射：STIP 对应 MTIP 的位需要移位映射。
29. **CSR 抽象层的价值在特权级迁移时体现**：阶段 0.5 设计的 `MINIOS_USE_S_MODE` 宏开关让所有内核代码通过一个编译 flag 即可从 M 模式切换到 S 模式，无需逐行修改 CSR 名称。回退只需改 Makefile 一行。这是"封装变化"原则在系统编程中的工程实践。
30. **开启分页后必须映射 MMIO 区域**：仅身份映射 DRAM 不足以让内核在 Sv39 模式下运行。UART/CLINT/PLIC 三个外设的地址（0x10000000/0x02000000/0x0C000000）不在 DRAM 范围内，未映射会导致 printk 等所有外设访问立即触发 PageFault。这是虚拟内存使能时的经典遗忘项。
31. **Sv39 页表使用 2MB 大页可显著简化映射**：128MB DRAM 只需 64 个 LV1 条目（每个覆盖 2MB），无需额外的 LV0 页表（4096 个 4KB 条目）。PLIC 64MB 只需 32 个 LV1 条目。总共 3 页（12KB）完成全部身份映射。这是空间效率与实现简单性的经典权衡。
32. **`.align` 在 GAS for RISC-V 中的语义**：`.align N` 表示 2^N 字节对齐。`.string` 自动 NUL 终止后长度不保证 4 字节对齐，必须显式对齐后续代码段，否则 RISC-V（无 C 扩展）取指会读取错位数据触发 IllegalInstruction。
33. **用户页表的 U 位控制整个子树**：非叶 PTE 的 U 位控制子树的用户可访问性。即使叶 PTE 设置了 U=1，如果上级非叶 PTE 的 U=0，用户访问仍会被拒绝。本实现中 `kernel_l1_mmio[0]` 设 U=1 允许遍历，叶级 PTE 各自控制（用户页 U=1，TEST_FINISH U=0）。
34. **sret 的行为**：`sret` 执行 `PC = sepc, mode = sstatus.SPP`，同时自动设 `SPP=User`（最低特权级）。进入用户模式后，只有 ecall 能回到 S 模式处理系统调用。
35. **ECALL 后 sepc 必须 +4**：用户态 ecall 触发 trap 后 sepc 指向 ecall 指令本身。trap handler 若不推进 sepc，sret 会回到同一条 ecall，形成无限循环。
36. **位置无关代码的关键**：用户程序使用 `auipc + addi`（`la` 伪指令）的 PC 相对寻址访问内嵌字符串，确保代码被复制到任意物理地址后仍能正确运行。这是用户态程序标准做法。
