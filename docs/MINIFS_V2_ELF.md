# MiniFS v2 与 ELF 用户程序

MiniOS 当前使用 8 MiB 持久化磁盘、MiniFS v2 和独立编译的 RV64I
用户 ELF。Shell、命令和测试程序不再作为内核汇编镜像内嵌。

## 构建与运行

```bash
cd ~/projects/mycpu
cmake --build build_wsl -j$(nproc)

cd os
make
make disk FORCE=1
make run
```

- `make`：构建内核和全部用户 ELF，不修改持久磁盘。
- `make disk`：从 `build/rootfs` 创建 `disk.img`；已有镜像时拒绝覆盖。
- `make disk FORCE=1`：重新格式化并覆盖旧镜像。
- `make run`：缺少磁盘时自动执行 `mkfs`，然后显式挂载 `os/disk.img`。
- `make clean`：保留 `disk.img`；`make distclean` 才删除磁盘。

## MiniFS v2

磁盘共 16384 个 512 B 扇区。固定布局如下：

| 区域 | 扇区 |
|---|---:|
| 超级块 | 0 |
| inode 位图 | 1 |
| 数据块位图 | 2-5 |
| 256 个 64 B inode | 6-37 |
| 数据区 | 38-16383 |

每个 inode 有 10 个直接块和 1 个一级间接块。一级间接块保存 128
个块号，文件大小额外强制限制为 64 KiB。目录项为 64 B，因此名称
最长 55 字节。路径最长 255 字节，支持绝对路径、相对路径、`.` 和
`..`。

文件系统支持创建、打开、关闭、读写、append、truncate、`lseek`、
创建目录、删除文件和空目录。删除根目录、非空目录或仍被打开的
inode 会失败。

## 文件描述符

每个进程有 16 个 fd，内核有 64 个共享 open-file description。
fd 0/1/2 默认连接 UART。`fork` 继承 fd 并增加引用计数，`execve`
保留 fd，进程退出时统一关闭。Shell 使用 `open + dup2` 实现：

```text
cat < input.txt
echo hello > output.txt
echo again >> output.txt
```

## ELF 加载

加载器接受 ELF64、小端、RISC-V、静态 `ET_EXEC`，只处理 `PT_LOAD`。
它校验 ELF 头、program header、文件边界、`filesz <= memsz`、入口
地址和用户地址范围，并按段的 R/W/X 权限建立 Sv39 映射。

用户地址从 `0x10000` 开始，四页用户栈顶部为 `0x200000`。加载器
复制文件内容并清零 BSS；`execve` 先构造完整的新地址空间，成功后
才替换旧地址空间。`fork` 深复制所有用户页。

初始栈保存 `argc, argv[], NULL, envp[], NULL, strings`，保持 16 B
对齐。`crt0.S` 从栈中解析参数后调用 C `main`。系统调用通过有界
`copy_from_user`、`copy_to_user` 和 `copy_string_from_user` 访问用户
内存。

## 用户程序

`/bin`：

```text
shell ls cat echo pwd ps kill env mkdir rm touch write
```

`/tests`：

```text
spin fstest forktest argtest
```

示例：

```text
ls /bin
mkdir /tmp/demo
write /tmp/demo/message.txt persistent data
cat /tmp/demo/message.txt
argtest hello "two words"
run spin &
ps
kill -15 PID
```

Shell 内建命令为 `help`、`cd`、`exit`、`exec` 和兼容命令 `run`。
其他命令都通过 `PATH=/bin:/tests` 搜索，并使用 `fork + execve`
执行。末尾 `&` 启动后台进程。

## 验证

```bash
ctest --test-dir build_wsl --output-on-failure
cd os
make clean && make
make clean && make BOOT_DIAGNOSTICS=1
make clean && make BOOT_COLOR=0
```

已验证 97 个 CTest 测试、精确 64 KiB 文件与间接块、超过 128 个
文件、三级以上目录、ELF 参数和环境、fork/exec/wait、重定向及跨
模拟器重启持久化。

## 当前边界

不支持动态链接、重定位、共享库、`mmap`、管道、符号链接、用户权限
模型、硬链接、日志和异常断电恢复。用户程序必须是静态 RV64I ELF，
单个磁盘文件不能超过 64 KiB。
