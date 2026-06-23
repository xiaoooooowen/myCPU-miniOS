#!/bin/bash
set -euo pipefail

# 构建 cemu 模拟器
cmake --build build_wsl --parallel 8

# 构建 MiniOS 内核与用户程序
cd os
make BOOT_COLOR=0
make install-kernel
cd ..
