# cemu — 自研 RISC-V RV64I 模拟器

cemu 是一个教学级 RISC-V RV64I 模拟器，负责提供 MiniOS 运行所需的硬件抽象。

## 功能模块

| 模块 | 描述 |
|------|------|
| CPU | RV64I 指令集模拟，支持 M/S/U 三种特权模式 |
| CSR | 控制状态寄存器，支持 mtvec、medeleg、mideleg、satp 等 |
| MMU | Sv39 页表转换与权限检查 |
| DRAM | 128 MB 内存模拟 |
| CLINT | 核级中断控制器（mtime/mtimecmp） |
| PLIC | 平台级中断控制器 |
| UART | 16550A 兼容串口，支持收发中断 |
| Block Device | 基于宿主文件的块设备，512B 扇区 |
| Bus | 内存映射总线仲裁与设备寻址 |

## 构建

见仓库根目录 `scripts/build_all.sh`。
