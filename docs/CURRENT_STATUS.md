# MiniOS / myCPU 项目当前状态

> 冻结时间：2026-05-29
> 冻结版本：阶段 0.5 全部完成 + 阶段 1 全部完成

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
timeout 2 ./build_wsl/cemu os/build/kernel.bin 2>/dev/null \
  | grep "at UART" \
  | python3 -c "import sys; sys.stdout.write(''.join(chr(int(x.strip(),16)) for x in sys.stdin.readlines()))"
```

**输出**：
```
MiniOS booting...
Hello from kernel!
```

- UART 写入次数：37 次（37 个字符，含换行）
- Fatal 异常：0 次
- 内核最终停在 `while(1){}` 死循环中正常运转

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
| MiniOS | os/ | ✅ 完成 | 启动汇编 + C 内核 + 链接脚本 + Makefile |
| CSR 抽象层 | os/include/csr.h | ✅ 完成 | CSRR/CSRW/CSRS/CSRC 宏封装 |

### 未完成的模块

| 模块 | 状态 |
|------|------|
| MMU / 页表 | ❌ 不存在 |
| 中断/异常处理 | ❌ ECALL/EBREAK 为空实现，trap 机制未建立 |
| 定时器中断 | ❌ CLINT 定时器中断未接入 CPU |
| shutdown 机制 | ❌ 内核只能 timeout 终止 |

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

## 七、已知问题

| 问题 | 严重性 | 状态 |
|------|--------|------|
| ~~B-type 分支指令符号扩展 bug~~ | 致命 | ✅ 已修复 |
| ~~JAL 立即数解码 bug~~ | 致命 | ✅ 已修复 |
| ~~UART 测试挂起~~ | 中 | ✅ 已修复 |
| ~~指令分派表缺失~~ | 致命 | ✅ 已修复 |
| MiniOS 完成后进入死循环 `while(1){}` 无法退出 | 低 | 预期行为，后续需要 shutdown 机制 |
| ECALL/EBREAK 目前为空实现 | 低 | 后续阶段实现 |
| 无中断/异常处理框架 | 中 | 阶段 2 任务 |

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
│   │   └── kernel.c         (内核 C 代码：UART 输出、kernel_main)
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
| P0 | 回顾阶段 2 任务定义，确定下一步方向 | 阶段 2 |
| P0 | 实现 trap 机制（mtvec/mepc/mcause/mstatus CSR 写入） | 阶段 2 |
| P1 | CLINT 定时器中断接入 CPU | 阶段 2 |
| P2 | 代码整洁：B-type 6 条指令提取公共立即数解码函数 | 重构 |
| P2 | 完善 ECALL 实现（至少支持 shutdown） | 阶段 2 |

## 十、本轮关键学习点

1. **unsigned 类型陷阱**：`auto` 推导纯整数运算结果为 unsigned。unsigned 上 `>>` 是逻辑右移（补 0），不会做符号扩展。这是 C/C++ 的经典陷阱。
2. **掩码宽度**：B-type 12 位立即数符号在 bit 12，需要 `0xFFFFF000`（20 位掩码），不是 `0xFFF00000`（12 位掩码）。
3. **MMIO 地址路由**：CPU 用 `load/store` 访问外设寄存器，无需独立 I/O 指令。这是 RISC-V 架构的核心理念。
4. **链接脚本**：裸机程序需要链接脚本定义 .text/.data/.bss 的加载地址和链接地址。
5. **forward declaration + unique_ptr**：打破 C++ 循环依赖的模式。
