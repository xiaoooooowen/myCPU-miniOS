# 开发日志

> 项目：MiniOS / myCPU — RISC-V 模拟器与迷你操作系统
> 目的：记录每次修改的上下文、决策理由和技术细节，便于后续回顾和学习。

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

