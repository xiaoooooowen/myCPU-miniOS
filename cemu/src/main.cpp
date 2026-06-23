#include <iostream>
#include <vector>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include "cpu.h"
#include "log.h"
#include "exception.h"
#include "param.h"

namespace {
volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) {
  stop_requested = 1;
}
}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2 && argc != 4) {
    LOG(cemu::ERROR, "Usage: cemu <boot.bin> [--disk <disk.img>]");
    return 1;
  }
  if (argc == 4 && std::string(argv[2]) != "--disk") {
    LOG(cemu::ERROR, "Usage: cemu <boot.bin> [--disk <disk.img>]");
    return 1;
  }

  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    LOG(cemu::ERROR, "Cannot open file: ", argv[1]);
    return 1;
  }

  std::vector<uint8_t> code(std::istreambuf_iterator<char>(file), {});
  std::filesystem::path disk_path =
      argc == 4 ? std::filesystem::path(argv[3])
                : std::filesystem::path(argv[1]).parent_path() / "disk.img";
  try {
    cemu::Cpu cpu(code, disk_path.string());
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

  // 启动 UART 标准输入监听，用于 OS 运行时读取终端输入
  cpu.bus.start_stdin();

  uint64_t instret = 0;
  const auto start_time = std::chrono::steady_clock::now();

  while (true) {
    if (stop_requested)
      break;

    // 检查是否被内核要求停机（TEST_FINISH 设备写入）
    if (cpu.bus.is_halted()) {
      LOG(cemu::INFO, "Simulation halted by kernel.");
      break;
    }

    // 推进 CLINT 时间，检测是否触发定时器中断
    if (cpu.bus.clint.tick()) {
      // mtime >= mtimecmp，设置机器定时器中断挂起位
      cpu.csr.store(cemu::MIP, cpu.csr.load(cemu::MIP) | cemu::MASK_MTIP);
    } else {
      // mtime < mtimecmp，清除机器定时器中断挂起位
      cpu.csr.store(cemu::MIP, cpu.csr.load(cemu::MIP) & ~cemu::MASK_MTIP);
    }

    // 在取指前检查是否有待处理的中断
    auto maybe_irq = cpu.check_pending_interrupts();
    if (maybe_irq.has_value()) {
      cpu.handle_interrupt(maybe_irq.value());
      continue;
    }

    try {
      auto inst = cpu.fetch();
      if (!inst.has_value()) {
        LOG(cemu::INFO, "End of program reached.");
        break;
      }
      auto new_pc = cpu.execute(inst.value());
      cpu.pc = new_pc.value();
      instret++;
    } catch (const cemu::Exception& e) {
      uint64_t fault_pc = cpu.pc;
      cpu.handle_exception(e);
      if (e.isFatal()) {
        LOG(cemu::WARNING, "Fatal error at PC 0x", std::hex, fault_pc,
            ": ", e.what(), std::dec);
        break;
      }
    }
  }

  const auto end_time = std::chrono::steady_clock::now();
  const std::chrono::duration<double> elapsed = end_time - start_time;
  const double seconds = elapsed.count();
  const double ips = seconds > 0.0
      ? static_cast<double>(instret) / seconds
      : 0.0;

  std::cout << "\n=== CEMU Performance ===\n"
            << "Instructions retired: " << instret << '\n'
            << std::fixed << std::setprecision(6)
            << "Elapsed time:         " << seconds << " s\n"
            << std::setprecision(2)
            << "Throughput:           " << ips << " IPS\n";

  cpu.dump_registers(); // 打印寄存器状态
  cpu.dump_pc();        // 打印PC寄存器状态

    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Cannot open disk image: " << error.what() << '\n';
    return 1;
  }
}
