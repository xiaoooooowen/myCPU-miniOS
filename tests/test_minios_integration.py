#!/usr/bin/env python3
"""MiniOS 系统级集成测试。

通过自动化 Shell 交互验证 MiniOS 的核心功能：
- 启动与 Shell 交互
- MiniFS 文件创建/读取/列出
- ELF 用户程序执行与参数传递
- fork/exec/wait 进程模型
- 文件系统压力测试
- 后台进程、ps、kill
- 磁盘持久化

前提条件：
    cmake --build build_wsl -j$(nproc)
    cd os && make clean && make BOOT_COLOR=0

用法：
    cd os && make clean && make BOOT_COLOR=0
    python3 tests/test_minios_integration.py
"""

import fcntl
import os as _os
import pathlib
import re
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
CEMU = ROOT / "build_wsl" / "cemu"
BOOT_BIN = ROOT / "os" / "build" / "boot.bin"
DISK_IMG = ROOT / "os" / "disk.img"
TEST_TIMEOUT = 20  # 单次测试超时秒数


# ---------------------------------------------------------------------------
# 辅助函数
# ---------------------------------------------------------------------------

def strip_ansi(text: str) -> str:
    """移除 ANSI 转义序列，保证无颜色模式下也能安全匹配。"""
    return re.sub(r"\x1b\[[0-9;]*[a-zA-Z]", "", text)


def has_program(program: str) -> bool:
    """检查系统 PATH 中是否存在指定程序。"""
    return shutil.which(program) is not None


# ---------------------------------------------------------------------------
# MiniOS 运行器
# ---------------------------------------------------------------------------

class MiniOSRunner:
    """管理 cemu 子进程的生命周期，提供命令发送和输出收集。

    MiniOS 的 Shell 提示符 ``minios:/> `` 不以换行结束，因此
    本类使用非阻塞 I/O 逐字符读取，避免 ``readline()`` 卡死。
    """

    READ_CHUNK = 256

    def __init__(self, disk_path: pathlib.Path):
        self._disk = disk_path
        self._process: subprocess.Popen | None = None
        self._output: str = ""
        self._seen: int = 0  # 已处理（匹配过）的输出字节数
        self._timed_out = False

    @property
    def disk(self) -> pathlib.Path:
        return self._disk

    def start(self) -> None:
        """启动 cemu 进程。"""
        if not CEMU.is_file():
            raise FileNotFoundError(f"cemu not found: {CEMU}")
        if not BOOT_BIN.is_file():
            raise FileNotFoundError(f"boot.bin not found: {BOOT_BIN}")
        if not self._disk.is_file():
            raise FileNotFoundError(f"disk image not found: {self._disk}")

        self._process = subprocess.Popen(
            [str(CEMU), str(BOOT_BIN), "--disk", str(self._disk)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        # 将 stdout 设为非阻塞
        if self._process.stdout is not None:
            fd = self._process.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | _os.O_NONBLOCK)
        if self._process.stderr is not None:
            fd = self._process.stderr.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | _os.O_NONBLOCK)

    def send(self, command: str) -> None:
        """向 MiniOS 发送一条命令（自动追加换行）。"""
        if self._process is None or self._process.stdin is None:
            raise RuntimeError("MiniOS not started")
        self._process.stdin.write((command + "\n").encode("utf-8"))
        self._process.stdin.flush()

    def read_until(self, *patterns: str, timeout: float = TEST_TIMEOUT) -> str:
        """非阻塞读取输出直到匹配到任意一个 pattern。

        参数 *patterns 中的每一项都会被 ``strip_ansi`` 后做子串匹配。
        只检查上次调用后新增的输出数据。
        超时或进程退出后返回已收集的全部输出。
        """
        if self._process is None or self._process.stdout is None:
            raise RuntimeError("MiniOS not started")
        fd = self._process.stdout.fileno()
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            if self._process.poll() is not None:
                self._drain_stdout()
                break

            # 先检查已有数据中是否有新增匹配
            clean = strip_ansi(self._output[self._seen:])
            for pat in patterns:
                if pat in clean:
                    self._seen = len(self._output)
                    return self._output

            # 用 select 等待可读
            remaining = max(0.0, deadline - time.monotonic())
            ready, _, _ = select.select([fd], [], [], min(0.1, remaining))
            if fd in ready:
                try:
                    data = _os.read(fd, self.READ_CHUNK)
                except BlockingIOError:
                    continue
                if not data:
                    break
                self._output += data.decode("utf-8", errors="replace")
            # select 超时，下一轮循环会检查新增数据
        else:
            self._timed_out = True
        # 最后再检查一次
        clean = strip_ansi(self._output[self._seen:])
        for pat in patterns:
            if pat in clean:
                self._seen = len(self._output)
                break
        return self._output

    def _drain_stdout(self) -> None:
        """进程退出后一次性排空 stdout。"""
        if self._process is None or self._process.stdout is None:
            return
        try:
            flags = fcntl.fcntl(self._process.stdout.fileno(), fcntl.F_GETFL)
            fcntl.fcntl(self._process.stdout.fileno(), fcntl.F_SETFL,
                        flags & ~_os.O_NONBLOCK)
            remaining = self._process.stdout.read()
            if remaining:
                self._output += remaining
        except Exception:
            pass

    def collect_remaining(self, timeout: float = 3.0) -> str:
        """在进程退出后收集剩余输出。"""
        if self._process is None:
            return self._output
        try:
            self._process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self._timed_out = True
        self._drain_stdout()
        return self._output

    def shutdown(self) -> tuple[int, str, str]:
        """终止 cemu 进程并返回 (returncode, stdout, stderr)。"""
        if self._process is None:
            return (-1, "", "")
        if self._process.poll() is None:
            # 先关闭 stdin，让 cemu 知道没有更多输入
            try:
                if self._process.stdin is not None:
                    self._process.stdin.close()
            except Exception:
                pass
            self._process.send_signal(signal.SIGTERM)
            try:
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait(timeout=3)
        self._drain_stdout()
        stderr_extra = ""
        try:
            if self._process.stderr is not None:
                flags = fcntl.fcntl(self._process.stderr.fileno(),
                                    fcntl.F_GETFL)
                fcntl.fcntl(self._process.stderr.fileno(), fcntl.F_SETFL,
                            flags & ~_os.O_NONBLOCK)
                stderr_extra = self._process.stderr.read() or ""
        except Exception:
            pass
        returncode = self._process.returncode if self._process.returncode is not None else -1
        full_stdout = self._output
        full_stderr = stderr_extra
        # 关闭剩余管道
        try:
            if self._process.stdout is not None:
                self._process.stdout.close()
        except Exception:
            pass
        try:
            if self._process.stderr is not None:
                self._process.stderr.close()
        except Exception:
            pass
        self._process = None
        return (returncode, full_stdout, full_stderr)

    @property
    def timed_out(self) -> bool:
        return self._timed_out


# ---------------------------------------------------------------------------
# 测试基类
# ---------------------------------------------------------------------------

class MiniOSIntegrationBase(unittest.TestCase):
    """为每个测试提供临时磁盘副本并自动清理 cemu 进程。"""

    def setUp(self):
        if not DISK_IMG.is_file():
            self.skipTest(f"disk.img not found: {DISK_IMG}")
        if not CEMU.is_file():
            self.skipTest(f"cemu not found: {CEMU}")
        if not BOOT_BIN.is_file():
            self.skipTest(f"boot.bin not found: {BOOT_BIN}")
        self._temp_dir = tempfile.TemporaryDirectory()
        self._temp_disk = pathlib.Path(self._temp_dir.name) / "disk.img"
        shutil.copy2(DISK_IMG, self._temp_disk)
        self._runner = MiniOSRunner(self._temp_disk)

    def tearDown(self):
        if hasattr(self, "_runner") and self._runner is not None:
            self._runner.shutdown()
        if hasattr(self, "_temp_dir"):
            self._temp_dir.cleanup()

    @property
    def runner(self) -> MiniOSRunner:
        return self._runner

    def assert_in_output(self, *patterns: str) -> None:
        """断言清理后的输出包含所有 pattern。"""
        clean = strip_ansi(self._runner._output)
        missing = [p for p in patterns if p not in clean]
        if missing:
            self.fail(
                f"Missing patterns: {missing}\n\n"
                f"--- Full output ---\n{self._runner._output}\n"
                f"--- End output ---"
            )

    def _check_clean_shutdown(self) -> None:
        """检查 cemu 正常退出（无超时，returncode 为 0）。"""
        self.assertFalse(self._runner.timed_out, "Test timed out")
        rc, stdout, stderr = self._runner.shutdown()
        if rc != 0:
            self.fail(
                f"cemu exited with code {rc}\n"
                f"--- stdout ---\n{stdout}\n"
                f"--- stderr ---\n{stderr}\n"
                f"--- End ---"
            )


# ---------------------------------------------------------------------------
# 测试用例 A: 启动与 Shell 测试
# ---------------------------------------------------------------------------

class TestBootAndShell(MiniOSIntegrationBase):
    def test_a_help_and_exit(self):
        """启动 MiniOS，执行 help 查看命令列表，然后 exit。"""
        self._runner.start()
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)
        self._runner.send("help")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)
        self._runner.send("exit")

        output = self._runner.read_until("Shell exited", timeout=TEST_TIMEOUT)
        self._runner.collect_remaining()

        self.assert_in_output(
            "Welcome to MiniOS",
            "minios:/>",
            "Built-in commands",
            "Shell exited",
        )
        self._check_clean_shutdown()


# ---------------------------------------------------------------------------
# 测试用例 B: 文件系统基础测试
# ---------------------------------------------------------------------------

class TestFileSystemBasics(MiniOSIntegrationBase):
    def test_b_file_operations(self):
        """创建目录、写入文件、读取文件、列出目录。"""
        self._runner.start()
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("mkdir /tmp/demo")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("write /tmp/demo/message.txt hello")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("cat /tmp/demo/message.txt")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("ls /tmp/demo")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("exit")
        self._runner.read_until("Shell exited", timeout=TEST_TIMEOUT)
        self._runner.collect_remaining()

        self.assert_in_output(
            "Welcome to MiniOS",
            "hello",
            "message.txt",
        )
        self._check_clean_shutdown()


# ---------------------------------------------------------------------------
# 测试用例 C: ELF 用户程序与参数测试
# ---------------------------------------------------------------------------

class TestArgTest(MiniOSIntegrationBase):
    def test_c_argtest_parameters(self):
        """运行 argtest 验证 argc/argv 传递。"""
        self._runner.start()
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send('argtest hello "two words"')
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("exit")
        self._runner.read_until("Shell exited", timeout=TEST_TIMEOUT)
        self._runner.collect_remaining()

        self.assert_in_output(
            "argc",
            "hello",
            "two words",
        )
        self._check_clean_shutdown()


# ---------------------------------------------------------------------------
# 测试用例 D: fork/exec/wait 测试
# ---------------------------------------------------------------------------

class TestForkTest(MiniOSIntegrationBase):
    def test_d_fork_exec_wait(self):
        """运行 forktest 验证 fork/execve/waitpid。"""
        self._runner.start()
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("forktest")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("exit")
        self._runner.read_until("Shell exited", timeout=TEST_TIMEOUT)
        self._runner.collect_remaining()

        self.assert_in_output("[forktest] PASS")
        self._check_clean_shutdown()


# ---------------------------------------------------------------------------
# 测试用例 E: 文件系统压力测试
# ---------------------------------------------------------------------------

class TestFSTest(MiniOSIntegrationBase):
    def test_e_fstest_stress(self):
        """运行 fstest 验证大文件写入与 seek。"""
        self._runner.start()
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("fstest")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("exit")
        self._runner.read_until("Shell exited", timeout=TEST_TIMEOUT)
        self._runner.collect_remaining()

        self.assert_in_output("[fstest] PASS")
        self._check_clean_shutdown()


# ---------------------------------------------------------------------------
# 测试用例 F: 后台进程、ps、kill 测试
# ---------------------------------------------------------------------------

class TestBackgroundPsKill(MiniOSIntegrationBase):
    def test_f_spin_ps_kill(self):
        """启动后台 spin，用 ps 查看，再用 kill 终止。"""
        self._runner.start()
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        # 启动后台 spin
        self._runner.send("spin &")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)
        self.assert_in_output("[pid")

        # 从输出中提取 PID
        clean = strip_ansi(self._runner._output)
        pid_match = re.search(r"\[pid\s+(\d+)\]", clean)
        if not pid_match:
            self.fail(
                f"Could not find [pid N] in output:\n"
                f"--- Full output ---\n{self._runner._output}\n"
                f"--- End output ---"
            )
        pid = pid_match.group(1)

        # ps 查看进程表
        self._runner.send("ps")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)
        self.assert_in_output("spin")

        # kill 终止 spin
        self._runner.send(f"kill -15 {pid}")
        self._runner.read_until("minios:/>", timeout=TEST_TIMEOUT)

        self._runner.send("exit")
        self._runner.read_until("Shell exited", timeout=TEST_TIMEOUT)
        self._runner.collect_remaining()

        self._check_clean_shutdown()


# ---------------------------------------------------------------------------
# 测试用例 G: 持久化测试
# ---------------------------------------------------------------------------

class TestPersistence(unittest.TestCase):
    """持久化测试需要同一个临时 disk.img 启动两次，因此不使用标准 setUp/tearDown。"""

    def setUp(self):
        if not DISK_IMG.is_file():
            self.skipTest(f"disk.img not found: {DISK_IMG}")
        if not CEMU.is_file():
            self.skipTest(f"cemu not found: {CEMU}")
        if not BOOT_BIN.is_file():
            self.skipTest(f"boot.bin not found: {BOOT_BIN}")
        self._temp_dir = tempfile.TemporaryDirectory()
        self._persist_disk = pathlib.Path(self._temp_dir.name) / "persist.img"
        shutil.copy2(DISK_IMG, self._persist_disk)

    def tearDown(self):
        if hasattr(self, "_temp_dir"):
            self._temp_dir.cleanup()

    def _run_session(self, commands: list[str],
                     expect: list[str], timeout: float = TEST_TIMEOUT):
        """启动一次 MiniOS，发送 commands 列表中的全部命令并等退出。"""
        runner = MiniOSRunner(self._persist_disk)
        runner.start()
        try:
            for cmd in commands:
                runner.read_until("minios:/>", timeout=timeout)
                if runner.timed_out:
                    break
                runner.send(cmd)
            runner.read_until("Shell exited", timeout=timeout)
            runner.collect_remaining()
        finally:
            runner.shutdown()

        clean = strip_ansi(runner._output)
        missing = [p for p in expect if p not in clean]
        if missing:
            self.fail(
                f"Persistence: missing patterns {missing}\n"
                f"--- Full output ---\n{runner._output}\n"
                f"--- End output ---"
            )

    def test_g_persistence(self):
        """第一次写入文件，第二次启动验证文件仍存在。"""
        self._run_session(
            commands=[
                "mkdir /tmp/persist",
                "write /tmp/persist/data.txt persistent",
                "exit",
            ],
            expect=["Welcome to MiniOS", "Shell exited"],
        )
        self._run_session(
            commands=[
                "cat /tmp/persist/data.txt",
                "exit",
            ],
            expect=["persistent", "Shell exited"],
        )


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    # 快速前置检查
    missing = []
    if not CEMU.is_file():
        missing.append(str(CEMU))
    if not BOOT_BIN.is_file():
        missing.append(str(BOOT_BIN))
    if not DISK_IMG.is_file():
        missing.append(str(DISK_IMG))
    if missing:
        print(f"ERROR: Required files not found: {', '.join(missing)}")
        print("Please run:")
        print("  cmake --build build_wsl -j$(nproc)")
        print("  cd os && make clean && make BOOT_COLOR=0")
        sys.exit(1)

    unittest.main(verbosity=2)
