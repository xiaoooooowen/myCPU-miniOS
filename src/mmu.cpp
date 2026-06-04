// mmu.cpp — Sv39 虚拟内存地址翻译实现
#include "mmu.h"
#include "exception.h"
#include "param.h"

namespace cemu {

Mmu::Mmu(Csr& csr, Dram& dram) : csr(csr), dram(dram) {}

bool Mmu::is_enabled() const {
  uint64_t satp = csr.load(SATP);
  return (satp & SATP_MODE_SV39) == SATP_MODE_SV39;
}

uint64_t Mmu::translate(uint64_t vaddr, AccessType type) {
  if (!is_enabled()) {
    return vaddr;  // 未开启分页，直通物理地址
  }

  static constexpr uint64_t VPN_MASK = 0x1FF;

  // 提取三级 VPN
  uint64_t vpn2 = (vaddr >> 30) & VPN_MASK;
  uint64_t vpn1 = (vaddr >> 21) & VPN_MASK;
  uint64_t vpn0 = (vaddr >> 12) & VPN_MASK;
  uint64_t offset = vaddr & 0xFFF;

  // 根页表物理地址 = SATP.PPN * PAGE_SIZE
  uint64_t satp = csr.load(SATP);
  uint64_t root_ppn = satp & 0xFFFFFFFFFFFULL;  // 低 44 位
  uint64_t table_addr = root_ppn * PAGE_SIZE;

  uint64_t pte_addr, pte;

  // Level 2 (根页表)
  pte_addr = table_addr + vpn2 * PTE_SIZE;
  auto pte_opt = dram.load(pte_addr, 64);
  if (!pte_opt.has_value())
    throw Exception(ExceptionType::LoadPageFault, vaddr);
  pte = pte_opt.value();

  if (!(pte & PTE_V))
    throw Exception(ExceptionType::LoadPageFault, vaddr);

  if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
    // 非叶节点 → 指向下一级页表
    table_addr = (pte >> 10) * PAGE_SIZE;
  } else {
    // 叶节点（1GB 大页，Sv39 支持但不常用）
    throw Exception(ExceptionType::LoadPageFault, vaddr);
  }

  // Level 1
  pte_addr = table_addr + vpn1 * PTE_SIZE;
  pte_opt = dram.load(pte_addr, 64);
  if (!pte_opt.has_value())
    throw Exception(ExceptionType::LoadPageFault, vaddr);
  pte = pte_opt.value();

  if (!(pte & PTE_V))
    throw Exception(ExceptionType::LoadPageFault, vaddr);

  uint64_t ppn;
  if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
    // 非叶节点 → 指向下一级页表
    table_addr = (pte >> 10) * PAGE_SIZE;
  } else {
    // 2MB 大页叶节点
    uint64_t leaf_ppn = (pte >> 10) & ~((1ULL << 9) - 1);  // PPN[2:1], 18 bits aligned
    ppn = leaf_ppn | (vpn0 & 0x1FF);  // vpn0 作为部分 offset
    // 检查权限
    if (type == AccessType::Instruction && !(pte & PTE_X))
      throw Exception(ExceptionType::InstructionPageFault, vaddr);
    if (type == AccessType::Load && !(pte & PTE_R))
      throw Exception(ExceptionType::LoadPageFault, vaddr);
    if (type == AccessType::Store && !(pte & PTE_W))
      throw Exception(ExceptionType::StoreAMOPageFault, vaddr);
    // 设置 A/D 位
    uint64_t new_pte = pte | PTE_A;
    if (type == AccessType::Store)
      new_pte |= PTE_D;
    dram.store(pte_addr, 64, new_pte);
    return (ppn * PAGE_SIZE) | offset;  // ppn * 4096 + offset
  }

  // Level 0 (叶节点，4KB 页)
  pte_addr = table_addr + vpn0 * PTE_SIZE;
  pte_opt = dram.load(pte_addr, 64);
  if (!pte_opt.has_value())
    throw Exception(ExceptionType::LoadPageFault, vaddr);
  pte = pte_opt.value();

  if (!(pte & PTE_V))
    throw Exception(ExceptionType::LoadPageFault, vaddr);

  // 权限检查
  if (type == AccessType::Instruction && !(pte & PTE_X))
    throw Exception(ExceptionType::InstructionPageFault, vaddr);
  if (type == AccessType::Load && !(pte & PTE_R))
    throw Exception(ExceptionType::LoadPageFault, vaddr);
  if (type == AccessType::Store && !(pte & PTE_W))
    throw Exception(ExceptionType::StoreAMOPageFault, vaddr);

  // 设置 A/D 位
  uint64_t new_pte = pte | PTE_A;
  if (type == AccessType::Store)
    new_pte |= PTE_D;
  dram.store(pte_addr, 64, new_pte);

  ppn = pte >> 10;  // PPN = PTE[53:10]
  return (ppn * PAGE_SIZE) | offset;
}

} // namespace cemu
