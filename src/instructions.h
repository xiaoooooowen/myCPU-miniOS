
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include "cpu.h"

namespace cemu {

class InstructionExecutor {
public:
  static std::optional<uint64_t> execute(Cpu& cpu, uint32_t inst);
};

}
