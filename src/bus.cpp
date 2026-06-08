// Bus.cpp
#include <string>
#include <iostream>
#include "bus.h"
#include "uart.h"
#include "param.h"
#include "log.h"
#include "exception.h"

namespace cemu {

Bus::Bus(const std::vector<uint8_t>& code)
    : dram(code),
      uart(std::make_unique<Uart>(false)),
      clint(),
      plic() {}

Bus::~Bus() = default;

Bus::Bus(Bus&&) = default;
Bus& Bus::operator=(Bus&&) = default;

std::optional<uint64_t> Bus::load(uint64_t addr, uint64_t size) {
  if (addr >= DRAM_BASE && addr <= DRAM_END) {
    LOG(INFO, "Bus loading from DRAM address ", std::hex, addr, " with size ", size, " bytes.");
    return dram.load(addr, size);
  }
  if (addr >= UART_BASE && addr <= UART_END) {
    LOG(INFO, "Bus loading from UART address ", std::hex, addr, " with size ", size, " bytes.");
    return uart->load(addr, size);
  }
  if (addr >= CLINT_BASE && addr <= CLINT_END) {
    LOG(INFO, "Bus loading from CLINT address ", std::hex, addr, " with size ", size, " bytes.");
    return clint.load(addr, size);
  }
  if (addr >= PLIC_BASE && addr <= PLIC_END) {
    LOG(INFO, "Bus loading from PLIC address ", std::hex, addr, " with size ", size, " bytes.");
    return plic.load(addr, size);
  }
  if (addr >= TEST_FINISH && addr <= TEST_FINISH_END) {
    LOG(INFO, "Bus loading from TEST_FINISH address ", std::hex, addr, " (halted=", halted, ").");
    return halted ? 1 : 0;
  }
  throw Exception(ExceptionType::LoadAccessFault, addr);
}

bool Bus::store(uint64_t addr, uint64_t size, uint64_t value) {
  if (addr >= DRAM_BASE && addr <= DRAM_END) {
    LOG(INFO, "Bus storing value ", std::hex, value, " at DRAM address ", addr, " with size ", size, " bytes.");
    return dram.store(addr, size, value);
  }
  if (addr >= UART_BASE && addr <= UART_END) {
    LOG(INFO, "Bus storing value ", std::hex, value, " at UART address ", addr, " with size ", size, " bytes.");
    uart->store(addr, size, value);
    return true;
  }
  if (addr >= CLINT_BASE && addr <= CLINT_END) {
    LOG(INFO, "Bus storing value ", std::hex, value, " at CLINT address ", addr, " with size ", size, " bytes.");
    clint.store(addr, size, value);
    return true;
  }
  if (addr >= PLIC_BASE && addr <= PLIC_END) {
    LOG(INFO, "Bus storing value ", std::hex, value, " at PLIC address ", addr, " with size ", size, " bytes.");
    plic.store(addr, size, value);
    return true;
  }
  if (addr >= TEST_FINISH && addr <= TEST_FINISH_END) {
    LOG(INFO, "Bus storing value ", std::hex, value, " at TEST_FINISH address ", addr, " -> halting simulation.");
    halted = true;
    return true;
  }
  throw Exception(ExceptionType::StoreAMOAccessFault, addr);
}


}