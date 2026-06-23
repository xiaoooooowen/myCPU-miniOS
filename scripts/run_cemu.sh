#!/bin/bash
set -euo pipefail

# TODO: 确认 cemu 运行命令
#
# 根据 os/Makefile 中的 run target，预期命令为：
#   build_wsl/cemu os/build/boot.bin --disk os/disk.img
#
# 但在实际运行前需要确认：
#   1. cemu 可执行路径是否正确（build_wsl/cemu）
#   2. 引导镜像路径（boot.bin vs kernel.bin，当前使用两阶段启动）
#   3. 磁盘镜像路径（os/disk.img）
#   4. 是否需要先自动构建（build + disk）
#
# 请用户确认后替换下方 TODO 命令。

echo "run_cemu.sh: 运行命令待确认，请编辑本脚本替换 TODO 行"
exit 1

# TODO: 替换为实际运行命令
# build_wsl/cemu os/build/boot.bin --disk os/disk.img
