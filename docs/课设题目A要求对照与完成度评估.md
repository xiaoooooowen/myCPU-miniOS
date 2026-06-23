# 课设题目 A 要求对照与 MiniOS 完成度评估

> 评估日期：2026-06-20  
> 评估对象：`myCPU` 自研 RV64I 模拟器与其上运行的 `MiniOS`  
> 题目来源：《操作系统课程设计》项目制方案（2025-2026 学年春季学期）A 方案  
> 评估口径：以仓库当前代码、构建产物、自动化测试和实际启动结果为准

## 2026-06-20 最终复评

> 本节替代下文 2026-06-18 快照中所有与 Bootloader 冲突的结论。

独立 Bootloader 已彻底完成：模拟器仅预装 `boot.bin` 到 `0x80000000`；
客体 M 态 Bootloader 通过块设备读取 `disk.img` 尾部内核槽，检查 magic、
版本、长度、加载地址、入口和 32 位校验和，将 `kernel.bin` 搬运到
`0x80200000` 后跳转。内核随后完成栈和 BSS 初始化、M 到 S 切换并进入
`kernel_main`。损坏任意一个内核字节会得到
`[BOOT] FAIL: checksum mismatch`，不会执行损坏镜像。

最终统计调整为：

```text
完成 40 条，部分完成 6 条，未完成 5 条
(40 + 6 x 0.5) / 51 = 84.3%
系统启动模块：7 / 7 = 100%
```

验证证据：

- Bootloader 入口 `0x80000000`，内核入口 `0x80200000`。
- 105/105 项 CTest 全部通过（含 98 项模拟器单元测试 + 7 项 MiniOS 系统级集成测试）。
- 正式 `os/disk.img` 已无损迁移并成功启动到 Shell。
- 详细实现见 [BOOTLOADER.md](BOOTLOADER.md)。

## 1. 结论摘要

MiniOS 已经明显超过题目 A 的基本要求：

- 题目基本要求是“至少 3 个模块、9 个功能点”。
- 当前项目覆盖题目列出的全部 6 个模块。
- 按本报告对题目中 51 条功能示例和技术指标逐条统计：
  - 完成：40 条
  - 部分完成：6 条
  - 未完成：5 条
- 若将“完成”计 1 分、“部分完成”计 0.5 分，严格完成度约为：

```text
(40 + 6 x 0.5) / 51 = 84.3%
```

这个比例采用较严格口径。例如：

- 模拟器在宿主侧将 `kernel.bin` 预装到 DRAM，只算“部分完成内核镜像加载”，不算完整客体 Bootloader。
- 模拟器能够产生页异常，但内核没有实现缺页修复和按需分页，因此不算完成缺页异常处理。
- UART 支持终端输入，但采用轮询读取而非外部中断，因此不算完成键盘中断。
- Shell 支持命令、参数、后台任务和重定向，但没有管道，因此管道指标不算完成。

当前最成熟的模块是：

1. 文件系统
2. 进程管理
3. 用户程序加载与执行
4. 系统调用与时钟中断

当前最需要补齐的项目是：

2. UART/键盘中断输入
3. 内核缺页异常处理和按需分页
4. `kmalloc/kfree` 字节级内核堆
5. Shell 管道
6. 用户程序异常退出与内核隔离验证

## 2. 评估方法与状态定义

### 2.1 状态定义

| 状态 | 含义 |
|---|---|
| 完成 | 有实际代码路径，能够构建运行，并存在测试、运行记录或清晰可复现证据 |
| 部分完成 | 已有底层能力或主要框架，但与题目描述存在关键语义差距，或缺少完整验证 |
| 未完成 | 未发现对应实现，或当前实现明确采用了不同机制，不能满足该项要求 |

### 2.2 本次验证结果

2026-06-18 实际执行：

```bash
cmake --build build_wsl
cd os && make
ctest --test-dir build_wsl --output-on-failure
cd os
printf "exit\n" | ../build_wsl/cemu build/kernel.bin --disk disk.img
```

结果：

- 模拟器与内核构建成功。
- CTest 共 106 项全部通过（98 项模拟器硬件单元测试 + 8 项 MiniOS 系统级集成测试）。
- MiniOS 成功从 M 态进入 S 态并进入 `kernel_main`。
- 物理内存、Sv39、MiniFS、调度器和用户 Shell 均报告 `[ OK ]`。
- Shell 能接收 `exit`，内核随后通过 `TEST_FINISH` 正常关闭模拟器。
- 本次启动没有 Fatal Exception、异常重启或死机。

实际启动关键输出：

```text
MiniOS | RV64I | S-mode | Sv39

[ OK ] Physical memory
[ OK ] Virtual memory
[ OK ] MiniFS
[ OK ] Scheduler
[ OK ] User shell

Welcome to MiniOS.
minios:/> exit
Shell exited, halting MiniOS.
```

## 3. 总体思路对照

题目 A 要求从“系统开发者”视角实现一个简化操作系统，核心包括程序加载、系统启动、内核管理和文件系统，并在模拟器中启动系统、执行命令。

当前项目与总体思路高度一致：

- 自研 RV64I CPU、CSR、MMU、总线和外设模拟器。
- 自研 RISC-V MiniOS 内核。
- M/S/U 三种特权级运行路径。
- Sv39 虚拟内存。
- 抢占式进程调度与系统调用。
- 持久化 MiniFS。
- ELF64 用户程序与 C Shell。
- 可在模拟器中启动并执行命令。

题目描述提到可使用 QEMU/Bochs 等现成模拟器。当前项目使用自研 `cemu`，这不是功能缺陷，反而具有一定创新性；但如果验收老师明确要求 QEMU 兼容，则还需要额外适配 QEMU `virt` 的启动协议和设备模型。目前仓库没有证明内核可直接在 QEMU 上启动。

## 4. 模块一：系统启动（Bootloader）

### 4.1 功能示例逐项对照

| 题目要求 | 状态 | 当前实现与证据 | 差距 |
|---|---|---|---|
| 从 M 态切换到 S 态 | 完成 | `_start` 设置 `mstatus.MPP=S`、`mepc=kernel_main`，执行 `mret` | 无关键差距 |
| 加载内核镜像到正确内存位置并跳转 | 完成 | M 态 Bootloader 从磁盘扇区 15360 读取镜像头，从 15361 起加载内核到 0x80200000，校验后跳转 | 已通过正常启动和损坏镜像拒绝测试 |
| 初始化中断向量表 | 完成 | `_start` 初始化 `mtvec=trap_entry_m` 和 `stvec=trap_entry`；S 态 trap 入口保存/恢复通用寄存器并执行 `sret` | 当前使用 Direct 模式单入口，不是 Vectored 模式，但满足 RISC-V 中断入口初始化要求 |
| 设置栈指针，为 C 环境做准备 | 完成 | 链接脚本定义 `_stack_top=0x88000000`，启动汇编执行 `la sp, _stack_top`，并清零 `.bss` | 未初始化 `gp`，但当前编译参数和运行结果未因此出现问题 |
| 输出启动日志，证明启动成功 | 完成 | `kernel_main` 输出 ASCII 横幅和内存、VM、MiniFS、调度器、Shell 状态 | 无关键差距 |

### 4.2 技术指标逐项对照

| 技术指标 | 状态 | 结论 |
|---|---|---|
| 能在模拟器中成功启动并进入内核主函数 | 完成 | Bootloader 入口为 0x80000000，内核入口为 0x80200000；实际启动成功 |
| 启动过程稳定可复现，无异常重启或死机 | 完成 | 本次构建和启动成功，Shell 正常退出并主动停机；105 项测试全部通过 |

### 4.3 Bootloader 的历史判定（已由顶部最终复评替代）

当前启动链路是：

```text
宿主程序 cemu
  -> 读取 kernel.bin
  -> std::copy 到模拟 DRAM 0x80000000
  -> CPU 初始 PC = 0x80000000，初始特权级 = M
  -> 执行内核自身的 _start
  -> 设置栈、清 BSS、设置 trap 向量
  -> mret 切换到 S 态
  -> kernel_main
```

严格意义的 Bootloader 链路应是：

```text
模拟器只预装 bootloader/Boot ROM
  -> 客体 Bootloader 通过块设备读取 kernel.img
  -> 校验镜像头、长度和校验和
  -> 搬运内核到目标物理地址
  -> 跳转内核 _start
  -> 内核完成 M 到 S 切换并进入 kernel_main
```

因此，本模块的启动、特权级切换、C 环境和日志都已完成，但“内核镜像加载”只能判为部分完成。

### 4.4 历史完成度（已由顶部最终复评替代）

按 7 条要求等权计算：

```text
6 项完成 + 1 项部分完成 = 6.5 / 7 = 92.9%
```

若将“独立加载内核”视为 Bootloader 的核心能力并提高权重，则工程意义上的完成度约为 80%-85%。

## 5. 模块二：中断与异常处理

### 5.1 功能示例逐项对照

| 题目要求 | 状态 | 当前实现与证据 | 差距 |
|---|---|---|---|
| 初始化与注册中断入口 | 完成 | 初始化 `mtvec/stvec`；`trap.S` 保存完整寄存器，`trap.c` 按 `scause` 分派 | 缺少通用动态 handler 注册表，但固定分派满足当前内核 |
| 时钟中断处理和定时功能 | 完成 | CLINT `mtime/mtimecmp`、STIE、中断委托、`timer_handle` 和 `sched_tick` 已接通 | 无关键差距 |
| 键盘中断和基本输入 | 未完成 | UART 支持 stdin 线程和内核轮询 `uart_getc` | 输入路径不是 UART 外部中断，没有扫描码处理 |
| `ecall` 系统调用和 U 到 S 切换 | 完成 | U 态 `ecall` 委托到 S 态；trap frame 读取 `a7/a0-a2` 并分派系统调用 | 无关键差距 |
| 缺页异常处理 | 部分完成 | MMU 能产生 instruction/load/store page fault，CSR 能记录异常 | 内核 `trap_handler` 没有针对 cause 12/13/15 的修复、分配、终止进程等处理 |

### 5.2 技术指标逐项对照

| 技术指标 | 状态 | 结论 |
|---|---|---|
| 时钟中断频率稳定，如 100Hz | 完成 | 默认时间片 10ms，即约 100Hz；`mtimecmp` 每次中断后重新设置 |
| 键盘中断读取扫描码并转换 ASCII | 未完成 | 当前 UART 直接提供字符流，且使用轮询，不存在扫描码转换 |
| 系统调用支持打印、进程创建、退出、文件读写 | 完成 | `write`、`fork`、`exit`、`open/read/write/close/lseek` 等均已实现 |

### 5.3 评价

本模块已经具备真实的 U/S trap、时钟中断和系统调用闭环，核心教学价值较高。主要不足是输入仍为轮询，以及 page fault 只有硬件/模拟器侧检测，没有内核侧策略。

建议完成度：

```text
5 项完成 + 1 项部分完成 + 2 项未完成
= 5.5 / 8
= 68.8%
```

## 6. 模块三：内存管理

### 6.1 功能示例逐项对照

| 题目要求 | 状态 | 当前实现与证据 | 差距 |
|---|---|---|---|
| 物理内存探测与可用区域识别 | 部分完成 | 按链接符号 `_kernel_end`、`_stack_top` 和固定 128MiB 布局计算可用页 | 没有读取设备树、BIOS/E820 或动态内存描述；属于固定平台内存规划 |
| 物理页框分配器 | 完成 | 4KiB 页，bump allocator + 空闲链表，`kalloc/kfree` 支持回收复用 | 没有伙伴算法，但题目允许简单分配器 |
| 页表创建、映射、解除映射 | 部分完成 | 内核建立 Sv39 页表；用户 ELF 按 R/W/X 映射；地址空间销毁时释放页 | 缺少通用 `map/unmap` 接口和运行期单页解除映射 API |
| 内核堆 `kmalloc/kfree` | 未完成 | 当前只有整页 `kalloc/kfree` | 没有任意字节大小分配器 |
| 用户空间代码、数据、堆、栈布局 | 完成 | ELF `PT_LOAD` 映射代码/数据，4 页用户栈，独立 SATP | 没有用户态 `brk/sbrk` 动态堆 |

### 6.2 技术指标逐项对照

| 技术指标 | 状态 | 结论 |
|---|---|---|
| 至少管理 4MB 物理内存 | 完成 | 当前 DRAM 为 128MiB |
| 页大小 4KB | 完成 | `PAGE_SIZE=4096`，用户页和页分配器均使用 4KiB |
| 支持按需分页 | 未完成 | ELF 段和用户栈在加载时预先分配，page fault 不执行懒分配 |
| 内存分配无泄漏，支持释放后重用 | 部分完成 | `kfree` 空闲链表支持重用，地址空间销毁会释放用户页；有诊断测试 | 尚无长期压力测试、泄漏统计器或形式化证明，内核任务栈采用静态策略 |

### 6.3 评价

当前已经完成课程项目中最关键的物理页分配、Sv39 地址翻译、权限隔离和独立用户地址空间。严格缺口是 Demand Paging 和字节级内核堆。

建议完成度：

```text
4 项完成 + 3 项部分完成 + 2 项未完成
= 5.5 / 9
= 61.1%
```

## 7. 模块四：进程管理

### 7.1 功能示例逐项对照

| 题目要求 | 状态 | 当前实现与证据 |
|---|---|---|
| PCB 数据结构 | 完成 | `task` 保存 PID/PPID、状态、上下文、SATP、内核栈、时间片、fd 关联信息 |
| `fork/exec` | 完成 | `fork` 深复制用户地址空间，`execve` 从 MiniFS 加载 ELF 并原子替换地址空间 |
| FCFS 和 RR 两种调度算法 | 完成 | `SCHED_FCFS`、`SCHED_RR` 可切换，默认 RR |
| 就绪、运行、阻塞、僵尸状态 | 完成 | `TASK_READY/RUNNING/BLOCKED/ZOMBIE` 已实现 |
| 信号量和互斥锁 | 完成 | `sem_init/wait/post` 与 `mutex_init/lock/trylock/unlock` |
| `exit/wait/waitpid` | 完成 | 僵尸状态、父进程唤醒、回收地址空间和 fd 均已实现 |

### 7.2 技术指标逐项对照

| 技术指标 | 状态 | 结论 |
|---|---|---|
| 支持多个并发进程 | 完成 | 最多 16 个任务，支持前台、后台和抢占 |
| RR 时间片可配置，默认 10ms | 完成 | `timer_set_timeslice_ms` 和 `task_set_quantum` 可配置，默认 10ms |
| 进程切换开销小于 1ms | 部分完成 | 内核记录最大切换 tick，并在诊断模式按 1ms 阈值判断 | 当前常规 CI 没有独立性能断言；自研模拟器时间也不完全等价于 QEMU 墙钟时间 |
| 支持父子进程关系树 | 完成 | PCB 有 `ppid`，支持孤儿接管和 `task_dump_tree` |

### 7.3 评价

这是当前完成度最高的核心内核模块之一。除“QEMU 中小于 1ms”的严格性能口径尚需专门测量外，题目要求基本全部实现。

建议完成度：

```text
9 项完成 + 1 项部分完成
= 9.5 / 10
= 95%
```

## 8. 模块五：文件系统

### 8.1 功能示例逐项对照

| 题目要求 | 状态 | 当前实现与证据 |
|---|---|---|
| 简化文件系统 | 完成 | 8MiB 持久化 MiniFS v2，含超级块、位图、inode 和数据块 |
| 文件/目录创建、删除、打开、关闭、读写 | 完成 | `open/close/read/write/mkdir/unlink` 完整接入系统调用 |
| 文件描述符表 | 完成 | 每进程 16 个 fd，内核共享 open-file description，支持继承和 close-on-exec |
| 绝对路径和相对路径解析 | 完成 | 支持 `/`、相对路径、`.`、`..` 和当前工作目录 |
| 标准输入输出重定向 | 完成 | Shell 使用 `open + dup2` 支持 `<`、`>`、`>>` |

### 8.2 技术指标逐项对照

| 技术指标 | 状态 | 结论 |
|---|---|---|
| 至少 128 个文件，单文件最大 64KB | 完成 | 256 inode；测试创建超过 128 个文件；严格限制单文件 64KiB |
| 至少 3 层目录 | 完成 | 自动化测试导入 `one/two/three` 及 130 个文件 |
| 支持 seek | 完成 | `lseek` 系统调用和 `fstest` 已覆盖 |
| 提供 mkfs 工具 | 完成 | `tools/mkfs_minifs.py` 生成 8MiB 镜像，支持安全覆盖选项 |

### 8.3 评价

文件系统模块完整覆盖题目功能示例和技术指标，而且比最低要求多出：

- 持久化块设备
- inode 位图与数据位图
- 直接块与一级间接块
- append/truncate
- cwd
- close-on-exec
- 跨进程 fd 继承
- ELF 文件直接从文件系统加载

建议完成度：

```text
9 / 9 = 100%
```

## 9. 模块六：用户程序加载与执行

### 9.1 功能示例逐项对照

| 题目要求 | 状态 | 当前实现与证据 |
|---|---|---|
| ELF 解析与加载 | 完成 | 支持 ELF64、小端、RISC-V、静态 `ET_EXEC` 和 `PT_LOAD`，校验文件边界与 R/W/X 权限 |
| 用户栈、参数和环境变量 | 完成 | 构造 `argc/argv/envp/strings`，保持 16B 对齐，`crt0.S` 调用 C `main` |
| 简化 libc | 完成 | 封装系统调用及字符串、内存、字符输出和 `printf` |
| Shell 与 ls/cat/echo/ps/kill/exec | 完成 | C Shell 支持 PATH 搜索、内建命令和外部 ELF |

### 9.2 技术指标逐项对照

| 技术指标 | 状态 | 结论 |
|---|---|---|
| 至少运行 5 个不同用户程序 | 完成 | `/bin` 有 shell、ls、cat、echo、pwd、ps、kill、env、mkdir、rm、touch、write 等 |
| Shell 支持命令解析和参数传递 | 完成 | 支持引号、转义、argv/envp、PATH、后台 `&` |
| Shell 支持管道 | 未完成 | 没有 `pipe` 系统调用、管道 fd 或 `cmd1 | cmd2` 解析 |
| 用户程序崩溃不影响内核稳定性 | 部分完成 | U/S 页权限隔离和独立地址空间已具备；非法用户指针在系统调用复制时可拒绝 | page fault/非法指令没有完整“终止当前进程并返回 Shell”的异常策略，也缺少专门崩溃测试 |

### 9.3 评价

用户程序加载、参数环境、libc 和 Shell 已经达到较完整的教学 OS 水平。剩余两个明显缺口是管道和用户异常隔离闭环。

建议完成度：

```text
6 项完成 + 1 项部分完成 + 1 项未完成
= 6.5 / 8
= 81.3%
```

## 10. 六大模块汇总

| 模块 | 完成 | 部分 | 未完成 | 加权完成度 |
|---|---:|---:|---:|---:|
| 系统启动 | 7 | 0 | 0 | 100% |
| 中断与异常 | 5 | 1 | 2 | 68.8% |
| 内存管理 | 4 | 3 | 2 | 61.1% |
| 进程管理 | 9 | 1 | 0 | 95.0% |
| 文件系统 | 9 | 0 | 0 | 100% |
| 用户程序 | 6 | 1 | 1 | 81.3% |
| **总计** | **40** | **6** | **5** | **84.3%** |

说明：

- 该统计按题目每个项目符号等权，仅用于项目自评。
- 课程实际评分还会考虑答辩、文档、演示、代码质量、创新点和教师对核心功能的权重判断。
- 自研 CPU 模拟器属于明显的额外工作量和创新点，但也需要准备好解释为何没有直接采用 QEMU。

## 11. 基本要求判断

题目管理评价中的基本要求是：

> 至少 3 个模块，9 个功能点。

当前项目保守统计也已经满足：

- 系统启动：至少 4 个完整功能点
- 中断与异常：至少 3 个完整功能点
- 内存管理：至少 3 个完整功能点
- 进程管理：至少 9 个完整功能点
- 文件系统：9 个完整功能点
- 用户程序：至少 6 个完整功能点

因此：

```text
基本要求：已显著超过
完整覆盖全部题目指标：尚未达到
当前适合状态：可以进入答辩材料整理和重点缺口补齐阶段
```

## 12. 主要缺口与优先级

### P0：建议答辩前优先处理

#### 12.1 独立 Bootloader 加载内核

原因：

- 这是“系统启动（Bootloader）”中唯一明确的核心缺口。
- 当前宿主加载容易被老师追问：“Bootloader 到底加载了什么？”
- 完成后可以形成非常清晰的演示链路。

推荐最小方案：

1. 增加 Boot ROM 或单独的 `boot.bin` 加载区。
2. 将内核链接地址调整到 `0x80200000`。
3. 磁盘保留固定内核区域，MiniFS 从后续扇区开始，或使用独立 kernel image。
4. Bootloader 通过块设备 MMIO 读取镜像头和内核扇区。
5. 检查 magic、加载地址、长度和校验和。
6. 搬运后跳转到内核 `_start`。

预计成本：

- 固定扇区裸二进制加载：1-2 天。
- 带镜像头和校验：2-4 天。
- Bootloader 直接解析 ELF：4-7 天。

#### 12.2 用户程序异常隔离

推荐行为：

- U 态非法指令、取指缺页、读缺页、写缺页时：
  - 打印 PID、`sepc/scause/stval`
  - 将当前进程标为 ZOMBIE
  - 切换到其他任务
  - Shell 继续运行
- S 态同类异常仍判为 kernel panic。

该功能工作量通常小于完整 Demand Paging，却能直接满足“用户程序崩溃不影响内核稳定性”。

### P1：明显提高完整度

#### 12.3 Shell 管道

需要：

- `pipe` 系统调用
- 内核管道环形缓冲区
- 阻塞读写和唤醒
- fork 后 fd 继承
- Shell 解析 `|`
- `dup2` 连接前后命令

完成后用户程序模块可接近满分。

#### 12.4 UART 中断输入

需要：

- UART RX 中断产生
- PLIC pending/claim/complete 完整路径
- S 态外部中断处理
- 内核输入缓冲区
- 阻塞 `read` 与任务唤醒

当前 UART 直接接收 ASCII，不必强行模拟 PC 键盘扫描码；答辩时应说明 RISC-V virt 风格串口输入与 x86 键盘控制器的差异。

### P2：扩展型指标

#### 12.5 Demand Paging

可以选择最小实现：

- ELF 加载时只建立 VMA 元数据，不立即分配所有页。
- 首次 instruction/load/store page fault 时分配物理页。
- 从 ELF 对应偏移读取内容并补零。
- 栈支持有限范围内的缺页增长。

该功能风险和测试成本较高，不建议在启动链路尚未补齐时优先实现。

#### 12.6 字节级 `kmalloc/kfree`

可以在 `kalloc/kfree` 之上增加：

- 固定尺寸 slab，如 32/64/128/256/512/1024/2048 字节。
- 或简单 free-list 堆分配器。

课程演示中页分配器已经具备较强说服力，字节级内核堆属于完善项。

## 13. 建议演示脚本

答辩演示可以按以下顺序进行：

```text
1. 启动 MiniOS
2. 展示 M -> S -> U 特权级链路
3. 展示启动 [OK] 日志
4. ls /bin
5. mkdir /tmp/demo
6. write /tmp/demo/message.txt hello MiniOS
7. cat /tmp/demo/message.txt
8. argtest hello "two words"
9. run spin &
10. ps
11. kill -15 <PID>
12. forktest
13. fstest
14. exit
15. 再次启动并 cat 持久化文件
```

如果独立 Bootloader 完成，应在第 1 步额外输出：

```text
[BOOT] block device ready
[BOOT] kernel image found
[BOOT] load 53248 bytes -> 0x80200000
[BOOT] checksum OK
[BOOT] jump to kernel
```

## 14. 答辩中需要准确表述的边界

建议使用以下表述：

### 可以明确声称已经完成

- M 态到 S 态切换。
- S 态内核和 U 态用户程序。
- 中断委托和时钟中断。
- 4KiB 物理页分配与回收复用。
- Sv39 页表和用户/内核权限隔离。
- FCFS 与 RR 调度。
- fork/exec/exit/wait/waitpid。
- 信号量和互斥锁。
- 持久化 MiniFS、目录、fd、seek、mkfs。
- ELF64 用户程序、参数、环境变量和简化 libc。
- C Shell、外部命令、后台进程和重定向。

### 不应声称已经完整完成

- 键盘/UART 中断输入。
- Demand Paging。
- 通用 `kmalloc/kfree`。
- Shell 管道。
- 完整的用户程序崩溃恢复。
- QEMU 兼容启动。

准确说明这些边界不会削弱项目，反而能体现对系统层次和实现语义的理解。

## 15. 关键代码证据索引

| 能力 | 文件 |
|---|---|
| M 到 S 启动、栈、BSS、向量入口 | `os/boot/start.S` |
| 内存布局和入口地址 | `os/linker.ld` |
| 独立 Bootloader 加载内核 | os/boot/loader_start.S、os/boot/loader.c、tools/install_kernel.py |
| Trap 入口和处理 | `os/kernel/trap.S`、`os/kernel/trap.c` |
| 时钟中断 | `os/kernel/timer.c`、`src/clint.cpp` |
| 物理页分配 | `os/kernel/mem.c` |
| Sv39 内核映射 | `os/kernel/vm.c` |
| 用户地址空间和 ELF | `os/kernel/user.c` |
| 进程和调度 | `os/kernel/task.c`、`os/kernel/switch.S` |
| 同步机制 | `os/kernel/sync.c` |
| 系统调用 | `os/kernel/syscall.c`、`os/user/libc.c` |
| MiniFS | `os/kernel/minifs.c`、`tools/mkfs_minifs.py` |
| Shell | `os/user/shell.c` |
| 自动化测试 | `tests/unitest/`、`tests/test_mkfs_minifs.py`、`tests/test_install_kernel.py`、`tests/test_minios_integration.py` |

## 16. 最终判断

MiniOS 当前不是只有若干孤立功能的演示程序，而是已经形成了可以实际启动、进入用户态、执行 ELF 程序、调度进程和持久化读写文件的教学操作系统。

从题目 A 的基本验收要求看，项目已经充分达标，并且在自研模拟器、完整用户 ELF、持久化文件系统和 C Shell 方面具有明显扩展。

从题目 A 全部 51 条示例和指标的严格口径看，当前约完成 84.3%。剩余工作并不平均分布，而是集中在少数明确模块：

```text
UART 中断输入
Page Fault 内核处理与 Demand Paging
kmalloc/kfree
Shell 管道
用户崩溃隔离
```

若答辩前只选择一个增强项，优先完成独立 Bootloader；若选择两个，再加入用户异常隔离。这样投入相对可控，同时最能补强题目原文中容易被追问的部分。

### 测试体系

项目已形成两层自动化测试覆盖：

1. **模拟器硬件单元测试**（98 项）：基于 Google Test，覆盖 RV64I 指令、CSR、DRAM、Bus、CPU、MMU、PLIC、CLINT、UART 等硬件模块，通过 `ctest` 运行。
2. **MiniOS 系统级集成测试**（8 项）：基于 Python `unittest`，通过 `subprocess.Popen` 驱动 cemu 与 MiniOS Shell 交互，黑盒验证启动、文件系统、ELF 程序、进程管理、一键测试套件、后台任务和持久化等 OS 核心功能。详见 `tests/test_minios_integration.py`。

两层测试互补：单元测试保证模拟器硬件正确性，集成测试证明操作系统整体行为符合预期。
