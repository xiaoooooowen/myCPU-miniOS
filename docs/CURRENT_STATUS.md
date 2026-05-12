# MiniOS / myCPU 项目当前状态

> 冻结时间：2026-05-11
> 冻结版本：阶段 0 起始状态

## 一、环境信息

| 项目 | 版本 / 路径 |
|------|-------------|
| OS | WSL (Linux) |
| 编译器 | GCC 13.3.0 |
| CMake | 4.2.3 |
| RISC-V 工具链 | riscv64-unknown-elf-gcc 13.2.0 (已安装) |
| RISC-V objcopy | riscv64-unknown-elf-objcopy (可用) |
| 测试框架 | Google Test (third_party/googletest) |
| 构建目录 | build_wsl/ |

## 二、构建状态

### 构建命令

```bash
cmake -B build_wsl -DCMAKE_BUILD_TYPE=Debug
cmake --build build_wsl -j$(nproc)
```

### 构建结果

| 目标 | 状态 | 编译警告 |
|------|------|----------|
| cemu (模拟器) | ✅ 通过 | 0 |
| unit_test (测试) | ✅ 通过 | 0 |
| common_library | ✅ 通过 | 0 |

### 产物

- `build_wsl/cemu` — 模拟器可执行文件
- `build_wsl/unit_test` — 单元测试可执行文件
- `build_wsl/libcommon_library.a` — 公共库

## 三、测试结果（2026-05-11 运行）

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
| PLIC 测试 (PlicTest) | 3 | 3 | 0 | 0 |
| CLINT 测试 (ClintTest) | 4 | 4 | 0 | 0 |
| UART 测试 (UartTest) | 5 | **挂起** | — | — |
| **合计（可自动运行）** | **60** | **60** | **0** | **0** |

### UART 测试异常

UART 测试在非交互环境（ctest）下全部挂起。根因：

- [uart.cpp](file:///home/xiaowen/projects/mycpu/src/uart.cpp#L10-L22) 构造函数启动了一个 `std::thread` 阻塞在 `std::cin >> byte`
- 该线程虽被 `detach()`，但在无 stdin 输入的环境下会无限阻塞
- 测试二进制 `UartTest.IsInterrupting` 启动后永远不返回

这是在交互终端下可正常运行但 CI/自动化测试环境会挂起的设计问题。

## 四、模块完成度

### 已完成的模块

| 模块 | 文件 | 状态 | 说明 |
|------|------|------|------|
| DRAM | dram.h/cpp | ✅ 完成 | 128MB 物理内存，8/16/32/64 位读写，小端序 |
| CSR | csr.h/cpp | ✅ 完成 | 4096 槽 CSR 数组，M/S 模式寄存器映射，medeleg/mideleg 委托逻辑 |
| Exception | exception.h/cpp | ✅ 完成 | 14 种 RISC-V 异常类型，isFatal 判断，格式化输出 |
| PLIC | plic.h/cpp | ✅ 完成 | 独立可测，pending/senable/spriority/sclaim 寄存器读写 |
| CLINT | clint.h/cpp | ✅ 完成 | 独立可测，mtime/mtimecmp 寄存器读写 |
| UART | uart.h/cpp | ✅ 完成 | 独立可测（交互环境），通过 stdin 线程代理输入，stdout 输出字符 |
| CPU | cup.h/cpp | ✅ 完成 | PC、32 通用寄存器、M/S/U 模式、fetch/execute/handle_exception |
| Instructions | instructions.h/cpp | ⚠️ 部分 | 见下方指令覆盖分析 |

### 未完成的模块

| 模块 | 状态 |
|------|------|
| Bus MMIO 路由 | ❌ 仅路由 DRAM，UART/CLINT/PLIC 未接入 |
| MMU / 页表 | ❌ 不存在 |
| OS 内核 | ❌ os/ 目录不存在 |

## 五、指令集覆盖分析

### 已实现且在分派表中的指令

| 类别 | 指令 | 状态 |
|------|------|------|
| 算术立即数 | ADDI, SLTI, SLTIU, XORI, ORI, ANDI | ✅ |
| 移位立即数 | SLLI, SRLI, SRAI | ✅ |
| 算术 | ADD, SUB (❌ 无), SLT, SLTU (❌ 无), XOR, OR, AND | ⚠️ |
| 移位 | SLL, SRL, SRA | ✅ |
| 加载 | LB, LH, LW, LD, LBU, LHU, LWU | ✅ |
| 存储 | SB, SD | ⚠️ SH/SW 实现但未分派 |
| 分支 | BEQ | ⚠️ BNE/BLT/BGE/BGEU 实现但未分派，BLTU 未实现 |
| 跳转 | JAL, JALR | ✅ |
| 高位立即数 | LUI, AUIPC | ✅ |
| CSR | CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI | ✅ |
| 特权 | SRET, MRET, SFENCE.VMA | ✅ |
| Fence | FENCE | ✅ |
| RV64I | ADDW | ⚠️ SUBW/SLLW/SRLW/SRAW 未实现 |

### 🔴 严重：指令分派表缺失条目

以下指令**实现函数已存在但未注册到分派表**，导致编译器生成这些指令时会触发 `IllegalInstruction`：

| 指令 | 实现函数 | 需要的分派项 | 影响 |
|------|----------|-------------|------|
| BNE | executeBNE | (0x63, 0x1) → 现为 BEQ，BNE 应是 (0x63, 0x0) | `if (a != b)` 会崩溃 |
| BLT | executeBLT | (0x63, 0x4) | `if (a < b)` 有符号比较崩溃 |
| BGE | executeBGE | (0x63, 0x5) | `if (a >= b)` 有符号比较崩溃 |
| BGEU | executeBGEU | (0x63, 0x7) | `if (a >= b)` 无符号比较崩溃 |
| SH | executeStoreHalf | (0x23, 0x1) | 16-bit 存储崩溃 |
| SW | executeStoreWord | (0x23, 0x2) | 32-bit 存储崩溃 |

### 🟡 中等：指令未实现

| 指令 | 需要的分派项 | 影响 |
|------|-------------|------|
| BLTU | (0x63, 0x6) | `if (a < b)` 无符号比较崩溃 |
| SUB | (0x33, 0x0, 0x20) | 减法操作崩溃 |
| SLTU | (0x33, 0x3, 0x00) | 无符号比较置位崩溃 |
| SUBW | (0x3b, 0x0, 0x20) | RV64 字减法崩溃 |
| SLLW | (0x3b, 0x1, 0x00) | RV64 字左移崩溃 |
| SRLW | (0x3b, 0x5, 0x00) | RV64 字逻辑右移崩溃 |
| SRAW | (0x3b, 0x5, 0x20) | RV64 字算术右移崩溃 |
| ADDIW | (0x1b, 0x0) | RV64 字立即数加法崩溃 |
| SLLIW | (0x1b, 0x1) | RV64 字立即数左移崩溃 |
| SRLIW | (0x1b, 0x5, 0x00) | RV64 字立即数逻辑右移崩溃 |
| SRAIW | (0x1b, 0x5, 0x20) | RV64 字立即数算术右移崩溃 |
| ECALL | (0x73, 0x0, 0x00) | 系统调用无法进入 |
| EBREAK | (0x73, 0x0, 0x01) | 断点调试无法使用 |
| WFI | (0x73, 0x0, 0x10) | 无法等待中断/优雅停机 |

### 编译建议

在补全上述指令前，MiniOS 内核编译应使用最保守的选项：

```makefile
CFLAGS = -march=rv64i -mabi=lp64 -ffreestanding -nostdlib -nostartfiles -O0
```

`-O0` 可以避免编译器生成 RV64I 扩展的 ADDW/SUBW 等字操作指令。

## 六、关键架构缺口（阻塞 MiniOS 启动）

### 1. Bus MMIO 路由（🔴 P0）

当前 [bus.cpp](file:///home/xiaowen/projects/mycpu/src/bus.cpp) 只识别 DRAM 地址范围，访问 UART/CLINT/PLIC 地址会直接抛异常。

| 设备 | 基地址 | 当前状态 |
|------|--------|----------|
| UART | 0x10000000 | 独立可测，Bus 不通 |
| CLINT | 0x02000000 | 独立可测，Bus 不通 |
| PLIC | 0x0c000000 | 独立可测，Bus 不通 |

### 2. 无 os/ 目录（🔴 P0）

项目总纲规划的 OS 目录结构完全不存在。

### 3. cup → cpu 文件名拼写（🟡 P1）

`cup.h/cup.cpp` 是 `Cpu` 类的文件，但拼写为 "cup"。影响 10+ 个 include 引用。

### 4. UART 访问位宽不一致（🟡 P1）

UART load/store 要求 `size == 8`（64-bit），但 NS16550 实际是 8-bit 寄存器。C 代码 `*(volatile uint8_t*)UART_THR = 'A'` 会生成 8-bit store（size=1），触发异常。

### 5. 无正常退出机制（🟡 P1）

`main.cpp` 主循环只在 fetch 失败或致命异常时退出。没有 WFI/ECALL 停机路径。

## 七、当前文件结构

```text
mycpu/
├── CMakeLists.txt
├── .gitignore
├── docs/
│   ├── 项目总纲.md          (+ v1.1 阶段 0.5 补充)
│   └── CURRENT_STATUS.md    (本文档)
├── src/
│   ├── main.cpp
│   ├── param.h              (地址常量、CSR 编号、掩码定义)
│   ├── log.h                (日志/彩色输出)
│   ├── cup.h / cup.cpp      (CPU 核心，⚠️ 文件名拼写)
│   ├── dram.h / dram.cpp    (物理内存)
│   ├── bus.h / bus.cpp      (总线，⚠️ 仅路由 DRAM)
│   ├── csr.h / csr.cpp      (控制状态寄存器)
│   ├── exception.h / .cpp   (异常建模)
│   ├── instructions.h / .cpp(指令执行，⚠️ 分派表不完整)
│   ├── plic.h / plic.cpp    (平台级中断控制器)
│   ├── clint.h / clint.cpp  (核心本地中断器)
│   └── uart.h / uart.cpp    (串口，⚠️ stdin 线程)
├── tests/
│   ├── test_util.h / .cpp   (测试辅助：汇编→二进制→CPU 运行)
│   └── unitest/
│       ├── instructions_test.cpp
│       ├── dram_test.cpp
│       ├── bus_test.cpp
│       ├── cpu_test.cpp
│       ├── csr_test.cpp
│       ├── exception.cpp
│       ├── plic_test.cpp
│       ├── clint_test.cpp
│       └── uart_test.cpp
└── third_party/
    └── googletest/
```

## 八、下一步行动清单（从阶段 0 到阶段 0.5）

按优先级排序：

| 优先级 | 编号 | 任务 | 阻塞 |
|--------|------|------|------|
| P0 | 1 | Bus 接入 UART MMIO | MiniOS 输出 |
| P0 | 2 | 创建 os/ 目录骨架 | MiniOS 结构 |
| P1 | 3 | 补全指令分派表缺失条目 | 内核编译后的代码执行 |
| P1 | 4 | cup → cpu 重命名 | 代码整洁 |
| P1 | 5 | UART 支持 8-bit 访问 | C 代码写 UART |
| P1 | 6 | WFI/ECALL 停机 | 内核优雅退出 |
| P2 | 7 | CMakeLists.txt 规范化 | 构建系统 |
| P2 | 8 | .gitignore 补充 OS 产物 | 版本控制 |
| P2 | 9 | 测试 include 路径规范化 | 可维护性 |
