# 测试基线报告 v1

> 基线版本：`minios-v1-course`
> 报告日期：2026-06-23

## 测试结果

**99/99 tests passed, 0 tests failed.**

### 测试分类

| 分类 | 数量 | 说明 |
|------|------|------|
| C++ GoogleTest 单元测试 (CpuTest, CsrTest, etc.) | 96 | 模拟器硬件模块单元测试 |
| Python 工具测试 (mkfs_minifs, install_kernel) | 2 | 文件系统镜像制作与内核安装 |
| 系统级集成测试 (minios_integration) | 1 | MiniOS Shell 黑盒交互测试 |

### 测试命令

```bash
cmake --build build_wsl --parallel 8
ctest --test-dir build_wsl --output-on-failure
```

### 全量测试耗时

36.53 秒（99 项）
