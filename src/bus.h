// Bus.h
#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "dram.h"
#include "clint.h"
#include "plic.h"

namespace cemu {

class Uart;

class Bus {
public:
  Bus(const std::vector<uint8_t>& code);
  ~Bus();

  Bus(const Bus&) = delete;
  Bus& operator=(const Bus&) = delete;
  Bus(Bus&&);
  Bus& operator=(Bus&&);

  std::optional<uint64_t> load(uint64_t addr, uint64_t size);
  bool store(uint64_t addr, uint64_t size, uint64_t value);

  bool is_halted() const { return halted; }

  /* 启动 UART 标准输入监听（用于 OS 运行时读取终端输入） */
  void start_stdin();

  Dram dram;
  std::unique_ptr<Uart> uart;
  Clint clint;
  Plic plic;

private:
  bool halted = false;
};

}
