# MiniOS 独立 Bootloader

> 最终状态：2026-06-20 完成并冻结

MiniOS 现在使用真正的两阶段启动链路。模拟器只负责把第一阶段
`boot.bin` 放入复位地址；运行在客体 RISC-V CPU M 态中的 Bootloader
自行访问块设备、读取内核镜像、校验、搬运并跳转。模拟器不再直接预装
`kernel.bin`。

## 启动链路

```text
cemu
  -> 仅加载 boot.bin 到 0x80000000
  -> CPU 从 0x80000000 以 M 态执行
  -> Bootloader 读取磁盘扇区 15360 的镜像头
  -> 校验 magic、版本、大小、加载地址和入口
  -> 从扇区 15361 起读取 kernel.bin
  -> 逐字节计算 32 位加法校验和
  -> 搬运内核到 0x80200000
  -> 校验成功后跳转到内核 _start
  -> 内核设置栈、清零 BSS、初始化 mtvec/stvec
  -> mret 从 M 态进入 S 态 kernel_main
  -> 初始化内存、Sv39、MiniFS、调度器和用户 Shell
```

## 内存布局

| 地址范围 | 用途 |
|---|---|
| `0x80000000-0x801fffff` | Bootloader，最大 2 MiB |
| `0x80200000-...` | MiniOS 内核镜像与 BSS |
| `...-0x87fbffff` | 物理页分配区 |
| `0x87fc0000-0x87ffffff` | 内核栈保护区 |
| `0x88000000` | 内核初始栈顶 |

Bootloader 栈顶为 `0x80200000`，栈向低地址增长。内核从该地址向高地址
加载，因此加载过程不会覆盖正在运行的 Bootloader 或其栈。

## 磁盘布局

整个块设备仍为 8 MiB，共 16384 个 512 B 扇区。

| 扇区 | 用途 |
|---|---|
| `0-15359` | MiniFS v2，可用容量 7.5 MiB |
| `15360` | 64 B 内核镜像头，其余填零 |
| `15361-16383` | 内核裸二进制，最大 523776 B |

MiniFS 超级块的 `blocks` 字段为 15360。数据块分配器不会进入内核槽，
数据位图也将尾部 1024 个扇区标记为保留。

## 内核镜像头

镜像头采用小端格式，总长 64 B：

```text
offset  size  field
0x00      8   magic = "MINIKRNL"
0x08      4   version = 1
0x0c      4   header_size = 64
0x10      8   load_address = 0x80200000
0x18      8   entry = 0x80200000
0x20      4   image_size
0x24      4   checksum
0x28     24   reserved
```

校验和为内核所有字节的无符号 32 位累加和：

```text
checksum = sum(kernel_bytes) mod 2^32
```

Bootloader 在跳转前验证：

- magic 与版本正确。
- 头长度为 64 B。
- 加载地址为 `0x80200000`。
- 内核非空且不超过内核槽容量。
- 入口位于已加载镜像范围内。
- 实际校验和与镜像头一致。

任一检查失败都会输出 `[BOOT] FAIL: ...` 并停在 M 态，不执行损坏内核。

## 构建与运行

```bash
cd ~/projects/mycpu
cmake --build build_wsl -j$(nproc)

cd os
make
make disk FORCE=1
make run
```

主要产物：

- `os/build/boot.elf`：带符号的 Bootloader ELF。
- `os/build/boot.bin`：模拟器唯一预装的启动代码。
- `os/build/kernel.elf`：链接到 `0x80200000` 的内核 ELF。
- `os/build/kernel.bin`：由 Bootloader 从磁盘加载的内核裸镜像。
- `os/disk.img`：MiniFS 与内核槽组合后的 8 MiB 启动磁盘。

`make run` 每次都会执行 `make install-kernel`，只更新尾部内核槽，不会
格式化 MiniFS，也不会删除用户创建的文件。

## 旧磁盘迁移

`tools/install_kernel.py` 支持旧版 MiniFS 镜像无损迁移：

1. 检查镜像为 8 MiB MiniFS v2。
2. 如果超级块仍声明 16384 个文件系统扇区，检查尾部 1024 扇区是否未分配。
3. 尾部未使用时，将超级块容量改为 15360，并在位图中标记保留区。
4. 尾部已被文件占用时拒绝迁移，不覆盖用户数据。
5. 写入镜像头和内核，并通过临时文件原子替换磁盘镜像。

正式磁盘迁移前的备份为：

```text
os/disk.img.pre-bootloader.bak
```

## 验证结果

2026-06-20 完成以下验证：

- Bootloader ELF 入口：`0x80000000`。
- Kernel ELF 入口：`0x80200000`。
- `boot.bin` 大小：1513 B。
- `kernel.bin` 大小：53360 B。
- 98/98 项 CTest 全部通过。
- 旧 `disk.img` 无损迁移成功。
- Bootloader 从磁盘加载内核并进入 Shell。
- Shell `exit` 后内核正常关闭模拟器。
- 人为损坏内核 1 字节后，Bootloader 输出
  `[BOOT] FAIL: checksum mismatch` 并拒绝跳转。

成功启动日志：

```text
[BOOT] MiniOS loader
[BOOT] reading kernel header
[BOOT] loading kernel -> 0x0000000080200000
[BOOT] checksum OK
[BOOT] jumping to kernel 0x0000000080200000
```

## 实现位置

| 内容 | 文件 |
|---|---|
| M 态 Bootloader 入口 | `os/boot/loader_start.S` |
| 块设备读取、校验、搬运和跳转 | `os/boot/loader.c` |
| Bootloader 内存布局 | `os/boot/loader.ld` |
| 内核内存布局 | `os/linker.ld` |
| 启动磁盘内核安装与迁移 | `tools/install_kernel.py` |
| MiniFS 保留区 | `tools/mkfs_minifs.py`、`os/kernel/minifs.c` |
| 镜像工具测试 | `tests/test_install_kernel.py` |
| 构建与运行入口 | `os/Makefile` |
