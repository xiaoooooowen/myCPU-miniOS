// clint.h

#pragma once
#include <cstdint>
#include "exception.h"
#include "param.h"

namespace cemu {

class Clint {
 public:
  Clint() : mtime(0), mtimecmp(0) {}

  uint64_t load(uint64_t addr, uint64_t size);
  void store(uint64_t addr, uint64_t size, uint64_t value);

  // 每个指令周期推进 mtime，并检查是否触发定时器中断
  // 返回 true 表示 mtime >= mtimecmp（应设置 MTIP）
  bool tick(uint64_t increment = 1);

  uint64_t get_mtime() const { return mtime; }
  uint64_t get_mtimecmp() const { return mtimecmp; }

 private:
  uint64_t mtime;     // Machine time
  uint64_t mtimecmp;  // Machine time compare
};

}