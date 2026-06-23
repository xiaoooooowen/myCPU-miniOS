#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cemu {

class BlockDevice {
 public:
  explicit BlockDevice(const std::string& image_path = "");

  uint64_t load(uint64_t addr, uint64_t size) const;
  void store(uint64_t addr, uint64_t size, uint64_t value);

 private:
  void execute(uint32_t command);
  void flush();
  void flush_sector(std::size_t offset);

  std::vector<uint8_t> storage;
  std::vector<uint8_t> window;
  std::filesystem::path path;
  uint64_t sector = 0;
  uint32_t status = 0;
};

}  // namespace cemu
