

#include <cstdint>
#include "csr.h"

namespace cemu {

// 构造函数
Csr::Csr() {
  csrs.fill(0); // 初始化所有CSR为0
}


// 打印所有的 CSR 寄存器
void Csr::dump_csrs() const {
  std::cout << std::setw(80) << std::setfill('-') << "control status registers" << std::setfill(' ') << "\n";
  std::cout << "mstatus = " << std::hex << std::setw(18) << load(MSTATUS)
            << "  mtvec = " << std::setw(18) << load(MTVEC)
            << "  mepc = " << std::setw(18) << load(MEPC)
            << "  mcause = " << std::setw(18) << load(MCAUSE) << "\n";
  std::cout << "sstatus = " << std::setw(18) << load(SSTATUS)
            << "  stvec = " << std::setw(18) << load(STVEC)
            << "  sepc = " << std::setw(18) << load(SEPC)
            << "  scause = " << std::setw(18) << load(SCAUSE) << "\n";
}

// 从 CSR 寄存器指定的位置中加载值
uint64_t Csr::load(size_t addr) const {
  switch (addr) {
    case SIE:
      return csrs[MIE] & csrs[MIDELEG];
    case SIP: {
      /*
       * 委托中断映射：SIP[2n+1] = MIP[2n+3] (n = 0, 2, 4)
       *   SSIP (bit 1) = MSIP (bit 3)
       *   STIP (bit 5) = MTIP (bit 7)
       *   SEIP (bit 9) = MEIP (bit 11)
       */
      uint64_t mideleg = csrs[MIDELEG];
      uint64_t mip = csrs[MIP];
      uint64_t sip = 0;
      if (mideleg & (1ULL << 1))  sip |= (mip & (1ULL << 3))  ? (1ULL << 1)  : 0;
      if (mideleg & (1ULL << 5))  sip |= (mip & (1ULL << 7))  ? (1ULL << 5)  : 0;
      if (mideleg & (1ULL << 9))  sip |= (mip & (1ULL << 11)) ? (1ULL << 9)  : 0;
      return sip;
    }
    case SSTATUS:
      return csrs[MSTATUS] & MASK_SSTATUS;
    default:
      return csrs[addr];
  }
}

// 将 value 存放到 CSR 寄存器指定的位置中
void Csr::store(size_t addr, uint64_t value) {
  switch (addr) {
    case SIE:
      csrs[MIE] = (csrs[MIE] & ~csrs[MIDELEG]) | (value & csrs[MIDELEG]);
    break;
    case SIP:
      csrs[MIP] = (csrs[MIP] & ~csrs[MIDELEG]) | (value & csrs[MIDELEG]);
    break;
    case SSTATUS:
      csrs[MSTATUS] = (csrs[MSTATUS] & ~MASK_SSTATUS) | (value & MASK_SSTATUS);
    break;
    default:
      csrs[addr] = value;
  }
}

// 这个函数的目的是检查 MEDELEG 寄存器中的某一位是否被设置。
// 在 RISC-V 架构中，MEDELEG 寄存器用于将某些中断从 M-mode
// 委托给 S-mode。如果 MEDELEG 寄存器中的某一位被设置，那么
// 对应的中断就被委托给了 S-mode。这个函数就是用来检查这种委
// 托状态的。
bool Csr::is_medelegated(uint64_t val) const {
  return (csrs[MEDELEG] >> val & 1) == 1;
}

// 这个函数的目的是检查 MIDELEG 寄存器中的某一位是否被设置。
// 在 RISC-V 架构中，MIDELEG 寄存器用于将某些中断从 M-mode
// 委托给 S-mode。如果 MIDELEG 寄存器中的某一位被设置，
// 那么对应的中断就被委托给了 S-mode。这个函数就是用来检查这种
// 委托状态的。
bool Csr::is_midelegated(uint64_t cause) const {
  return (csrs[MIDELEG] >> cause & 1) == 1;
}

}