# MiniOS / myCPU

RISC-V RV64I 模拟器（cemu）+ 教学操作系统（MiniOS）项目。

## 项目结构

```
mycpu/
├── cemu/                 # 自研 RISC-V RV64I 模拟器
│   └── src/              # 模拟器源码（CPU、CSR、MMU、DRAM、UART 等）
├── os/                   # MiniOS 操作系统
│   ├── boot/             # 两阶段启动引导（Bootloader）
│   ├── kernel/           # 内核源码
│   ├── user/             # 用户态程序（Shell、ls、cat 等）
│   ├── include/          # 内核公共头文件
│   ├── platform/         # 平台抽象层（框架预留）
│   ├── Makefile
│   └── linker.ld
├── tests/
│   ├── cemu/             # 模拟器测试
│   │   ├── unit/unitest/ # GoogleTest 单元测试
│   │   └── riscv/        # RISC-V 指令测试
│   ├── os/integration/   # MiniOS 系统级集成测试
│   └── tools/            # 镜像工具测试
├── tools/                # 构建工具（mkfs_minifs、install_kernel）
├── scripts/              # 构建/测试/运行脚本
├── third_party/          # 第三方依赖（GoogleTest）
├── artifacts/            # 生成物（报告、日志、截图等，不纳入版本控制）
│   ├── reports/
│   ├── logs/
│   └── screenshots/
└── docs/                 # 项目文档
    ├── 00_overview/
    ├── 01_design/
    ├── 02_reports/
    │   └── course_design/    # 课程设计报告 + 源码
    ├── 03_logs/
    └── archive/
```

## 构建

```bash
./scripts/build_all.sh
```

构建内容：
- `build_wsl/cemu` — RISC-V 模拟器
- `os/build/boot.bin` — 两阶段启动引导
- `os/build/kernel.bin` — MiniOS 内核
- `os/disk.img` — MiniFS 磁盘镜像（含用户程序）

## 测试

```bash
./scripts/test_all.sh
```

执行 99 项 CTest，包含：
- 96 项 C++ GoogleTest 模拟器硬件单元测试（`tests/cemu/unit/`）
- 2 项 Python 工具测试（`tests/tools/`）
- 1 项 MiniOS 系统级集成测试（`tests/os/integration/`）

## 运行

```bash
./scripts/run_cemu.sh
```

> 当前运行命令待确认，详见 `scripts/run_cemu.sh` 中的 TODO。

## 文档

参见 [docs/README.md](docs/README.md)。

## 基线

课程设计基线标记：`minios-v1-course`
