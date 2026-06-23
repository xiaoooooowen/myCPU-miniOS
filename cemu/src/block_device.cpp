#include "block_device.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "exception.h"
#include "param.h"

namespace cemu {

namespace {
constexpr uint64_t REG_SECTOR = 0x00;
constexpr uint64_t REG_COMMAND = 0x08;
constexpr uint64_t REG_STATUS = 0x0c;
constexpr uint64_t DATA_WINDOW = 0x100;
constexpr uint32_t CMD_READ = 1;
constexpr uint32_t CMD_WRITE = 2;
constexpr uint32_t STATUS_READY = 0;
constexpr uint32_t STATUS_ERROR = 1;

uint64_t read_le(const std::vector<uint8_t>& data, std::size_t offset,
                 uint64_t size) {
  const uint64_t bytes = size / 8;
  uint64_t value = 0;
  for (uint64_t i = 0; i < bytes; ++i)
    value |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
  return value;
}

void write_le(std::vector<uint8_t>& data, std::size_t offset, uint64_t size,
              uint64_t value) {
  const uint64_t bytes = size / 8;
  for (uint64_t i = 0; i < bytes; ++i)
    data[offset + i] = static_cast<uint8_t>(value >> (i * 8));
}
}  // namespace

BlockDevice::BlockDevice(const std::string& image_path)
    : storage(BLOCK_SECTOR_SIZE * BLOCK_SECTOR_COUNT, 0),
      window(BLOCK_SECTOR_SIZE, 0),
      path(image_path) {
  if (path.empty())
    return;

  if (std::filesystem::exists(path)) {
    if (std::filesystem::file_size(path) != storage.size())
      throw std::runtime_error("disk image must be exactly 8 MiB");
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(storage.data()),
               static_cast<std::streamsize>(storage.size()));
    if (!input)
      throw std::runtime_error("failed to read disk image");
    return;
  }

  const auto parent = path.parent_path();
  if (!parent.empty())
    std::filesystem::create_directories(parent);
  flush();
}

uint64_t BlockDevice::load(uint64_t addr, uint64_t size) const {
  if (size != 8 && size != 16 && size != 32 && size != 64)
    throw Exception(ExceptionType::LoadAccessFault, addr);

  const uint64_t offset = addr - BLOCK_BASE;
  if (offset == REG_SECTOR && size == 64)
    return sector;
  if (offset == REG_STATUS && size == 32)
    return status;
  if (offset >= DATA_WINDOW) {
    const uint64_t index = offset - DATA_WINDOW;
    if (index + size / 8 <= window.size())
      return read_le(window, index, size);
  }
  throw Exception(ExceptionType::LoadAccessFault, addr);
}

void BlockDevice::store(uint64_t addr, uint64_t size, uint64_t value) {
  if (size != 8 && size != 16 && size != 32 && size != 64)
    throw Exception(ExceptionType::StoreAMOAccessFault, addr);

  const uint64_t offset = addr - BLOCK_BASE;
  if (offset == REG_SECTOR && size == 64) {
    sector = value;
    return;
  }
  if (offset == REG_COMMAND && size == 32) {
    execute(static_cast<uint32_t>(value));
    return;
  }
  if (offset >= DATA_WINDOW) {
    const uint64_t index = offset - DATA_WINDOW;
    if (index + size / 8 <= window.size()) {
      write_le(window, index, size, value);
      return;
    }
  }
  throw Exception(ExceptionType::StoreAMOAccessFault, addr);
}

void BlockDevice::execute(uint32_t command) {
  if (sector >= BLOCK_SECTOR_COUNT) {
    status = STATUS_ERROR;
    return;
  }

  const std::size_t offset = sector * BLOCK_SECTOR_SIZE;
  if (command == CMD_READ) {
    std::copy_n(storage.begin() + offset, BLOCK_SECTOR_SIZE, window.begin());
    status = STATUS_READY;
  } else if (command == CMD_WRITE) {
    std::copy_n(window.begin(), BLOCK_SECTOR_SIZE, storage.begin() + offset);
    flush_sector(offset);
    status = STATUS_READY;
  } else {
    status = STATUS_ERROR;
  }
}

void BlockDevice::flush() {
  if (path.empty())
    return;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(storage.data()),
               static_cast<std::streamsize>(storage.size()));
  output.flush();
  if (!output)
    throw std::runtime_error("failed to write disk image");
}

void BlockDevice::flush_sector(std::size_t offset) {
  if (path.empty())
    return;
  std::fstream output(path, std::ios::binary | std::ios::in | std::ios::out);
  output.seekp(static_cast<std::streamoff>(offset));
  output.write(reinterpret_cast<const char*>(storage.data() + offset),
               BLOCK_SECTOR_SIZE);
  output.flush();
  if (!output)
    throw std::runtime_error("failed to write disk image sector");
}

}  // namespace cemu
