// mmu_test.cpp — MMU Sv39 地址翻译单元测试
#include <gtest/gtest.h>
#include "../../src/mmu.h"
#include "../../src/csr.h"
#include "../../src/dram.h"
#include "../../src/exception.h"

namespace cemu {

// 测试夹具：为页表测试准备 Csr + Dram
class MmuTest : public ::testing::Test {
protected:
  static constexpr uint64_t PAGE_SIZE = 4096;
  static constexpr uint64_t PTE_SIZE = 8;

  // PTE 标志位
  static constexpr uint64_t PTE_V = 1ULL << 0;
  static constexpr uint64_t PTE_R = 1ULL << 1;
  static constexpr uint64_t PTE_W = 1ULL << 2;
  static constexpr uint64_t PTE_X = 1ULL << 3;
  static constexpr uint64_t PTE_U = 1ULL << 4;

  // 页表物理地址
  static constexpr uint64_t ROOT_TABLE_ADDR = 0x80001000;
  static constexpr uint64_t LV1_TABLE_ADDR = 0x80002000;
  static constexpr uint64_t LV0_TABLE_ADDR = 0x80003000;

  // 测试用的虚拟地址和物理地址
  static constexpr uint64_t TEST_VA = 0x80000000;
  static constexpr uint64_t TEST_PA = 0x80000000;

  std::vector<uint8_t> empty_code{};  // 空代码，Dram 需要
  Dram dram{empty_code};
  Csr csr{};
  Mmu mmu{csr, dram};

  void SetUp() override {
    // 默认 csr 全为零，SATP.MODE = Bare
  }

  // 辅助：写 PTE 到页表
  void write_pte(uint64_t table_addr, uint64_t index, uint64_t pte) {
    dram.store(table_addr + index * PTE_SIZE, 64, pte);
  }

  // 辅助：构建 Sv39 身份映射 VA → PA
  // 映射一个 4KB 页
  void setup_identity_map(uint64_t va, uint64_t pa, uint64_t flags) {
    uint64_t vpn2 = (va >> 30) & 0x1FF;
    uint64_t vpn1 = (va >> 21) & 0x1FF;
    uint64_t vpn0 = (va >> 12) & 0x1FF;

    // 根页表：vpn2 → LV1 table
    uint64_t root_pte = ((LV1_TABLE_ADDR / PAGE_SIZE) << 10) | PTE_V;
    write_pte(ROOT_TABLE_ADDR, vpn2, root_pte);

    // LV1 页表：vpn1 → LV0 table
    uint64_t lv1_pte = ((LV0_TABLE_ADDR / PAGE_SIZE) << 10) | PTE_V;
    write_pte(LV1_TABLE_ADDR, vpn1, lv1_pte);

    // LV0 页表：vpn0 → 物理页
    uint64_t lv0_pte = ((pa / PAGE_SIZE) << 10) | flags | PTE_V;
    write_pte(LV0_TABLE_ADDR, vpn0, lv0_pte);
  }

  // 开启 Sv39 分页
  void enable_sv39() {
    // SATP: MODE=Sv39(8), PPN=root_table
    uint64_t satp = (8ULL << 60) | (ROOT_TABLE_ADDR / PAGE_SIZE);
    csr.store(SATP, satp);
  }
};

// 测试 1：Bare 模式，地址直通
TEST_F(MmuTest, BareModePassThrough) {
  EXPECT_FALSE(mmu.is_enabled());
  EXPECT_EQ(mmu.translate(0x80000000, Mmu::AccessType::Load, Supervisor), 0x80000000);
  EXPECT_EQ(mmu.translate(0x10000000, Mmu::AccessType::Load, Supervisor), 0x10000000);
  EXPECT_EQ(mmu.translate(0x00000000, Mmu::AccessType::Load, Supervisor), 0x00000000);
}

// 测试 2：Sv39 身份映射，只读页
TEST_F(MmuTest, Sv39IdentityMapRead) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R);
  enable_sv39();

  EXPECT_TRUE(mmu.is_enabled());
  uint64_t paddr = mmu.translate(TEST_VA, Mmu::AccessType::Load, Supervisor);
  EXPECT_EQ(paddr, TEST_PA);
}

// 测试 3：Sv39 身份映射，可读写页
TEST_F(MmuTest, Sv39IdentityMapReadWrite) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R | PTE_W);
  enable_sv39();

  uint64_t paddr = mmu.translate(TEST_VA, Mmu::AccessType::Load, Supervisor);
  EXPECT_EQ(paddr, TEST_PA);

  paddr = mmu.translate(TEST_VA, Mmu::AccessType::Store, Supervisor);
  EXPECT_EQ(paddr, TEST_PA);
}

// 测试 4：Sv39 身份映射，可执行页
TEST_F(MmuTest, Sv39IdentityMapExecute) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R | PTE_X);
  enable_sv39();

  uint64_t paddr = mmu.translate(TEST_VA, Mmu::AccessType::Instruction, Supervisor);
  EXPECT_EQ(paddr, TEST_PA);
}

// 测试 5：重映射到不同物理地址
TEST_F(MmuTest, Sv39Remap) {
  uint64_t remap_pa = 0x81000000;
  setup_identity_map(TEST_VA, remap_pa, PTE_R | PTE_W);
  enable_sv39();

  uint64_t paddr = mmu.translate(TEST_VA, Mmu::AccessType::Load, Supervisor);
  EXPECT_EQ(paddr, remap_pa);
}

// 测试 6：页内偏移保持不变
TEST_F(MmuTest, Sv39PageOffsetPreserved) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R | PTE_W);
  enable_sv39();

  for (uint64_t off : {0x0ULL, 0x100ULL, 0xFFFULL, 0x3ABULL}) {
    uint64_t va = TEST_VA | off;
    uint64_t pa = mmu.translate(va, Mmu::AccessType::Load, Supervisor);
    EXPECT_EQ(pa, TEST_PA | off);
  }
}

// 测试 7：无效 PTE（V=0）触发 LoadPageFault
TEST_F(MmuTest, InvalidPTEPageFault) {
  // 只设置根页表和 LV1 指向 LV0，但 LV0 未写入（V=0）
  uint64_t vpn2 = (TEST_VA >> 30) & 0x1FF;
  uint64_t vpn1 = (TEST_VA >> 21) & 0x1FF;

  uint64_t root_pte = ((LV1_TABLE_ADDR / PAGE_SIZE) << 10) | PTE_V;
  write_pte(ROOT_TABLE_ADDR, vpn2, root_pte);

  uint64_t lv1_pte = ((LV0_TABLE_ADDR / PAGE_SIZE) << 10) | PTE_V;
  write_pte(LV1_TABLE_ADDR, vpn1, lv1_pte);

  enable_sv39();

  EXPECT_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Load, Supervisor), Exception);
}

// 测试 8：无写权限时 store 触发 StoreAMOPageFault
TEST_F(MmuTest, StoreOnReadOnlyPageCausesPageFault) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R);  // 只读
  enable_sv39();

  EXPECT_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Store, Supervisor), Exception);
}

// 测试 9：无可执行权限时取指触发 InstructionPageFault
TEST_F(MmuTest, FetchOnNoExecutePageCausesPageFault) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R | PTE_W);  // 无 X
  enable_sv39();

  EXPECT_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Instruction, Supervisor), Exception);
}

// 测试 10：2MB 大页映射
TEST_F(MmuTest, Sv39Megapage) {
  uint64_t va = 0x80000000;
  uint64_t pa = 0x80000000;
  uint64_t vpn2 = (va >> 30) & 0x1FF;
  uint64_t vpn1 = (va >> 21) & 0x1FF;

  // 根页表：vpn2 → LV1 table
  uint64_t root_pte = ((LV1_TABLE_ADDR / PAGE_SIZE) << 10) | PTE_V;
  write_pte(ROOT_TABLE_ADDR, vpn2, root_pte);

  // LV1 为 2MB 大页叶节点 (R=1, W=1)
  // PTE bits 53:10 = 完整 PPN (PPN[0] 必须为 0，VA 的 vpn0 会替换它)
  uint64_t full_ppn = pa / PAGE_SIZE;  // 对于 pa=0x80000000, PPN=0x80000
  uint64_t lv1_pte = (full_ppn << 10) | PTE_V | PTE_R | PTE_W;
  write_pte(LV1_TABLE_ADDR, vpn1, lv1_pte);

  enable_sv39();

  uint64_t paddr = mmu.translate(va, Mmu::AccessType::Load, Supervisor);
  EXPECT_EQ(paddr, pa);
}

// 测试 11：U 位检查 — 用户模式访问 U=0 页触发页异常
TEST_F(MmuTest, UserModeCannotAccessSupervisorPage) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R | PTE_W);  // U=0
  enable_sv39();

  // S/M 模式访问：正常
  EXPECT_NO_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Load, Supervisor));
  EXPECT_NO_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Load, Machine));

  // U 模式访问 U=0 页：触发页异常
  EXPECT_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Load, User), Exception);
  EXPECT_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Store, User), Exception);
  EXPECT_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Instruction, User), Exception);
}

// 测试 12：U 模式可以正常访问 U=1 页
TEST_F(MmuTest, UserModeCanAccessUserPage) {
  setup_identity_map(TEST_VA, TEST_PA, PTE_R | PTE_W | PTE_U);  // U=1
  enable_sv39();

  EXPECT_NO_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Load, User));
  EXPECT_NO_THROW(mmu.translate(TEST_VA, Mmu::AccessType::Store, User));
}

// 测试 13：U 位检查 — 2MB 大页
TEST_F(MmuTest, UserModeMegapageUCheck) {
  uint64_t va = 0x80000000;
  uint64_t pa = 0x80000000;
  uint64_t vpn2 = (va >> 30) & 0x1FF;
  uint64_t vpn1 = (va >> 21) & 0x1FF;

  uint64_t root_pte = ((LV1_TABLE_ADDR / PAGE_SIZE) << 10) | PTE_V;
  write_pte(ROOT_TABLE_ADDR, vpn2, root_pte);

  // 2MB 大页，U=0
  uint64_t full_ppn = pa / PAGE_SIZE;
  uint64_t lv1_pte = (full_ppn << 10) | PTE_V | PTE_R | PTE_W;  // 无 U 位
  write_pte(LV1_TABLE_ADDR, vpn1, lv1_pte);

  enable_sv39();

  // S 模式可访问
  EXPECT_NO_THROW(mmu.translate(va, Mmu::AccessType::Load, Supervisor));
  // U 模式不可访问
  EXPECT_THROW(mmu.translate(va, Mmu::AccessType::Load, User), Exception);
}

}  // namespace cemu
