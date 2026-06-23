#!/bin/bash
set -euo pipefail

# 构建 cemu 模拟器及测试
cmake --build build_wsl --parallel 8

# 运行 CTest
ctest --test-dir build_wsl --output-on-failure
