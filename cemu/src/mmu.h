// mmu.h — Sv39 虚拟内存地址翻译
#pragma once

#include <cstdint>
#include "dram.h"
#include "csr.h"

namespace cemu {

class Mmu {
public:
  enum class AccessType { Instruction, Load, Store };

  Mmu(Csr& csr, Dram& dram);

  // 翻译虚拟地址到物理地址，失败抛 PageFault 异常
  // mode: 当前 CPU 特权级（User=0, Supervisor=1, Machine=3），用于 U 位权限检查
  uint64_t translate(uint64_t vaddr, AccessType type, uint64_t mode);

  // 检查 SATP 是否已开启分页
  bool is_enabled() const;

private:
  Csr& csr;
  Dram& dram;

  static constexpr uint64_t PAGE_SIZE = 4096;
  static constexpr uint64_t PTE_SIZE = 8;

  // PTE 标志位
  static constexpr uint64_t PTE_V = 1ULL << 0;
  static constexpr uint64_t PTE_R = 1ULL << 1;
  static constexpr uint64_t PTE_W = 1ULL << 2;
  static constexpr uint64_t PTE_X = 1ULL << 3;
  static constexpr uint64_t PTE_U = 1ULL << 4;
  static constexpr uint64_t PTE_G = 1ULL << 5;
  static constexpr uint64_t PTE_A = 1ULL << 6;
  static constexpr uint64_t PTE_D = 1ULL << 7;

  static constexpr uint64_t SATP_MODE_SV39 = 8ULL << 60;

  // 三级页表遍历
  uint64_t walk(uint64_t vaddr, AccessType type);
};

} // namespace cemu
