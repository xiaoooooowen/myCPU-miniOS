#include <gtest/gtest.h>
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

}  // namespace cemu