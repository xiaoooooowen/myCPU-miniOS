# MiniOS / myCPU 项目当前状态

> 冻结时间：2026-06-03
> 冻结版本：阶段 0.5 + 阶段 1 + 阶段 2 + 阶段 3 + 阶段 4 + 阶段 5 全部完成

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

## 三、测试结果（2026-05-29 运行）

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
| 自定义指令测试 | 2 | 2 | 0 | 0 |
| **合计** | **78** | **78** | **0** | **0** |

### MiniOS 运行验证

```bash
timeout 2 ./build_wsl/cemu os/build/kernel.bin > /tmp/cemu_out.log 2>&1
python3 -c "
import re
with open('/tmp/cemu_out.log') as f:
    chars = [chr(int(m.group(1),16)) for line in f if (m := re.search(r'storing value ([0-9a-f]+) at UART', line))]
print(''.join(chars))
"
```

**输出**：
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
--- Phase 3: ECALL Trap Test ---
Triggering ECALL to test trap handler...
=== TRAP ===
mcause: 0xa
mepc:   0x8000011c
mtval:  0x73
Type: Exception (0xa)
  -> Environment call from M-mode
=== TRAP END ===
Returned from trap handler!
Trap round-trip successful!
--- Phase 4: Timer Interrupt Test ---
Timer initialized: mtime=0x4e83, mtimecmp=0x506c
=== TRAP ===
mcause: 0x8000000000000007
mepc:   0x80000054
mtval:  0x0
Type: Interrupt (0x7)
  -> Machine timer interrupt
Tick #1 @ mtime=0x7186
=== TRAP END ===
Tick #2 @ mtime=0x9363
Tick #3 @ mtime=0xb54e
... (周期性触发)
```

- ECALL 触发 trap → handler 解码 mcause=0xa → mepc+4 → mret 返回 → 继续执行
- 定时器中断周期性触发（约每 500 cycles），中断 handler 打印 tick 计数后 mret 回到 wfi 循环
- 中断 mcause 带 bit63 置位（0x8000000000000007），异常 mcause 不带（0xa）
- 内存分配器：102KB 内核区域占 24 页，剩余 32702 页可分配。bump+free_list 混合算法，O(1) 初始化，分配/释放均页对齐。释放后重新分配回到同一地址

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
| 指令集 | instructions.h/cpp | ✅ 完成 | 约 50 条 RV64I 指令，dispatch table 完整 |
| MiniOS 裸机程序 | os/boot/, os/kernel/ | ✅ 完成 | 启动汇编 + C 内核 + 链接脚本 + Makefile |
| MiniOS UART 驱动 | os/kernel/uart.h/c | ✅ 完成 | uart_init/uart_putc/uart_puts 分离为独立模块 |
| MiniOS 内核日志 | os/kernel/printk.h/c | ✅ 完成 | 支持 %s/%d/%x/%lx/%c/%%，免除法实现避免依赖 libgcc |
| CSR 抽象层 | os/include/csr.h | ✅ 完成 | CSRR/CSRW/CSRS/CSRC 宏封装，含 trap 别名宏（TRAP_VEC/EPC/CAUSE/TVAL/STATUS） |
| Trap 入口汇编 | os/kernel/trap.S | ✅ 完成（阶段 3） | 保存全部 32 个寄存器到栈帧，调用 C handler，恢复后 mret |
| Trap handler | os/kernel/trap.h/c | ✅ 完成（阶段 3） | 读取 mcause/mepc/mtval，区分异常/中断，timer 中断调用 timer_handle() |
| 定时器驱动 | os/kernel/timer.h/c | ✅ 完成（阶段 4） | CLINT MMIO 读写，mtimecmp 设置，MTIE 使能，tick 计数 |
| CPU 中断检测 | cpu.h/cpp | ✅ 完成（阶段 4） | check_pending_interrupts() + handle_interrupt() |
| 物理内存分配器 | os/kernel/mem.h/c | ✅ 完成（阶段 5） | bump allocator + 空闲链表混合，kalloc/kfree/mem_free_pages |

### 未完成的模块

| 模块 | 状态 |
|------|------|
| MMU / 页表 | ❌ 不存在 |
| shutdown 机制 | ❌ 内核只能 timeout 终止 |
| 多任务调度 | ❌ 阶段 6+ |
| 系统调用 | ❌ 阶段 8+ |
| S 模式 / 用户态 | ❌ 阶段 9+ |

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
| MiniOS 完成后无法主动退出（仅 timeout 终止） | 低 | 预期行为，后续需要 shutdown 机制 |
| WFI 指令为 NOP 实现，无法真正暂停 CPU | 低 | 功能正确但效率低，不影响当前阶段 |

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
│   ├── uart.h / uart.cpp    (NS16550A UART 模型)
│   ├── clint.h / clint.cpp  (CLINT 定时器模型)
│   └── plic.h / plic.cpp    (PLIC 中断控制器模型)
├── os/
│   ├── Makefile             (交叉编译：riscv64-unknown-elf-gcc)
│   ├── linker.ld            (链接脚本：OUTPUT_ARCH(riscv), 0x80000000)
│   ├── boot/
│   │   └── start.S          (启动汇编：设栈、清零 BSS、跳转 kernel_main)
│   ├── kernel/
│   │   ├── kernel.c         (内核入口：UART 输出 + printk 演示 + ECALL trap 测试)
│   │   ├── uart.h           (UART 驱动头文件)
│   │   ├── uart.c           (UART 驱动：uart_putc/uart_puts)
│   │   ├── printk.h         (内核日志头文件)
│   │   ├── printk.c         (内核日志：%s/%d/%x/%lx/%c/%%，免除法)
│   │   ├── trap.S           (trap 入口汇编：寄存器保存/恢复 + mret)
│   │   ├── trap.h           (trap handler 头文件)
│   │   ├── trap.c           (trap handler：读取 mcause/mepc/mtval，区分异常/中断)
│   │   ├── timer.h          (定时器驱动头文件)
│   │   ├── timer.c          (定时器驱动：CLINT MMIO 读写 + tick 计数)
│   │   ├── mem.h            (物理内存分配器头文件)
│   │   └── mem.c            (物理内存分配器：bump+free_list 混合，kalloc/kfree)
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
│       └── exception.cpp
└── third_party/
    └── googletest/
```

## 九、下一步行动

| 优先级 | 任务 | 所属阶段 |
|--------|------|----------|
| P0 | 任务结构与协作式调度 | 阶段 6 |
| P0 | 上下文切换汇编 (switch.S) | 阶段 6 |
| P1 | 抢占式调度（基于定时器中断） | 阶段 7 |
| P2 | 代码整洁：B-type 6 条指令提取公共立即数解码函数 | 重构 |
| P2 | 完善 shutdown 机制（通过 sifive_test 或自定义 halt） | 未来阶段 |

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
