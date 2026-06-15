#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "../../src/bus.h"
#include "../../src/param.h"
#include "../../src/exception.h"

namespace cemu {

class BusTest : public ::testing::Test {
protected:
  std::vector<uint8_t> code = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  Bus bus = Bus(code);
};

TEST_F(BusTest, LoadTest) {
  auto value = bus.load(DRAM_BASE, 64);
  ASSERT_TRUE(value.has_value());
  ASSERT_EQ(value.value(), 0x0807060504030201);
}

TEST_F(BusTest, StoreTest) {
  uint64_t store_value = 0x0102030405060708;
  ASSERT_TRUE(bus.store(DRAM_BASE, 64, store_value));

  auto load_value = bus.load(DRAM_BASE, 64);
  ASSERT_TRUE(load_value.has_value());
  ASSERT_EQ(load_value.value(), store_value);
}

TEST_F(BusTest, InvalidLoadTest) {
  EXPECT_THROW(bus.load(DRAM_BASE, 10), Exception);
}

TEST_F(BusTest, InvalidStoreTest) {
  EXPECT_THROW(bus.store(DRAM_BASE, 10, 0x01), Exception);
}

TEST_F(BusTest, UartMMIOLoadStore) {
  uint64_t addr = UART_BASE + UART_LCR;
  bus.store(addr, 8, 0xAB);
  auto value = bus.load(addr, 8);
  ASSERT_TRUE(value.has_value());
  ASSERT_EQ(value.value(), 0xAB);
}

TEST_F(BusTest, UartMMIOStoreOutputsToStdout) {
  testing::internal::CaptureStdout();
  bus.store(UART_BASE + UART_THR, 8, 'H');
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find('H') != std::string::npos);
}

TEST_F(BusTest, ClintMMIOLoadStore) {
  bus.store(CLINT_MTIME, 64, 0xDEADBEEF);
  auto value = bus.load(CLINT_MTIME, 64);
  ASSERT_TRUE(value.has_value());
  ASSERT_EQ(value.value(), 0xDEADBEEF);
}

TEST_F(BusTest, ClintMMIOMtimecmp) {
  bus.store(CLINT_MTIMECMP, 64, 0x12345678);
  auto value = bus.load(CLINT_MTIMECMP, 64);
  ASSERT_TRUE(value.has_value());
  ASSERT_EQ(value.value(), 0x12345678);
}

TEST_F(BusTest, PlicMMIOLoadStore) {
  bus.store(PLIC_PENDING, 32, 0xCAFE);
  auto value = bus.load(PLIC_PENDING, 32);
  ASSERT_TRUE(value.has_value());
  ASSERT_EQ(value.value(), 0xCAFE);
}

TEST_F(BusTest, InvalidAddressException) {
  EXPECT_THROW(bus.load(0x50000000, 64), Exception);
  EXPECT_THROW(bus.store(0x50000000, 64, 0), Exception);
}

TEST_F(BusTest, BlockDeviceReadWrite) {
  constexpr uint64_t sector = 7;
  bus.store(BLOCK_BASE, 64, sector);
  bus.store(BLOCK_BASE + 0x100, 64, 0x1122334455667788ULL);
  bus.store(BLOCK_BASE + 0x08, 32, 2);

  bus.store(BLOCK_BASE + 0x100, 64, 0);
  bus.store(BLOCK_BASE + 0x08, 32, 1);
  auto value = bus.load(BLOCK_BASE + 0x100, 64);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), 0x1122334455667788ULL);
  EXPECT_EQ(bus.load(BLOCK_BASE + 0x0c, 32).value(), 0);
}

TEST_F(BusTest, BlockDeviceRejectsInvalidSector) {
  bus.store(BLOCK_BASE, 64, BLOCK_SECTOR_COUNT);
  bus.store(BLOCK_BASE + 0x08, 32, 1);
  EXPECT_EQ(bus.load(BLOCK_BASE + 0x0c, 32).value(), 1);
}

TEST_F(BusTest, BlockDevicePersistsAcrossMounts) {
  const auto path =
      std::filesystem::temp_directory_path() / "cemu-block-persistence.img";
  std::filesystem::remove(path);
  {
    Bus persistent_bus(code, path.string());
    persistent_bus.store(BLOCK_BASE, 64, 11);
    persistent_bus.store(BLOCK_BASE + 0x100, 64, 0xA1B2C3D4E5F60718ULL);
    persistent_bus.store(BLOCK_BASE + 0x08, 32, 2);
  }
  {
    Bus persistent_bus(code, path.string());
    persistent_bus.store(BLOCK_BASE, 64, 11);
    persistent_bus.store(BLOCK_BASE + 0x08, 32, 1);
    EXPECT_EQ(persistent_bus.load(BLOCK_BASE + 0x100, 64).value(),
              0xA1B2C3D4E5F60718ULL);
  }
  std::filesystem::remove(path);
}

TEST_F(BusTest, BlockDeviceRejectsWrongSizedImage) {
  const auto path =
      std::filesystem::temp_directory_path() / "cemu-block-invalid.img";
  {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.put('\0');
  }
  EXPECT_THROW(Bus(code, path.string()), std::runtime_error);
  EXPECT_EQ(std::filesystem::file_size(path), 1);
  std::filesystem::remove(path);
}

}  // namespace cemu
