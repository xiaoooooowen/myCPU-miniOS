// uart.h

#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include "exception.h"
#include "param.h"

namespace cemu {

class Uart {
 public:
  explicit Uart(bool start_stdin_listener = false);
  ~Uart();
  Uart(const Uart&) = delete;
  Uart& operator=(const Uart&) = delete;

  void start_stdin_listener();

  bool is_interrupting();
  uint64_t load(uint64_t addr, uint64_t size);
  void store(uint64_t addr, uint64_t size, uint64_t value);

  std::vector<uint8_t> uart;
  std::condition_variable cv;
  std::atomic<bool> interrupt;
  std::mutex mtx;

 private:
  std::thread stdin_thread;
  std::atomic<bool> stdin_running;
  void stdin_listener();
};

}