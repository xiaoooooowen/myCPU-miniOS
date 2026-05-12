#include "test_util.h"
#include "../src/log.h"
#include <filesystem>

namespace cemu {

static const std::string kRiscvTestDir = "tests/riscv-tests/";

void generate_rv_assembly(const std::string& c_src) {
  std::string command = "riscv64-unknown-elf-gcc -S" + c_src + " -o ";
  int result = std::system(command.c_str());

  if (result != 0) {
    throw std::runtime_error("Failed to generate RV assembly. Command: " + command);
  }
}

void generate_rv_obj(const std::string& assembly) {
  size_t dotPos = assembly.find_last_of(".");
  std::string baseName = (dotPos == std::string::npos) ? assembly : assembly.substr(0, dotPos);

  std::string command = "riscv64-unknown-elf-gcc -Wl,-Ttext=0x0 -nostdlib -o " + baseName + " " + assembly;

  int result = std::system(command.c_str());

  if (result != 0) {
    std::cerr << "Failed to generate RV object from assembly: " << assembly << std::endl;
  }
}

void generate_rv_binary(const std::string& obj) {
  std::string command = "riscv64-unknown-elf-objcopy -O binary " + obj + " " + obj + ".bin";

  int result = std::system(command.c_str());

  if (result != 0) {
    std::cerr << "Failed to generate RV binary from object: " << obj << std::endl;
  }
}

Cpu rv_helper(const std::string& code, const std::string& testname, size_t n_clock) {
  std::filesystem::create_directories(kRiscvTestDir);

  std::string filepath = kRiscvTestDir + testname + ".s";
  std::ofstream file(filepath);
  if (!file.is_open()) {
    LOG(ERROR, "Failed to create assembly file.");
  }
  file << code;
  file.close();

  generate_rv_obj(filepath.c_str());
  generate_rv_binary((kRiscvTestDir + testname).c_str());

  std::string binFilepath = kRiscvTestDir + testname + ".bin";
  std::ifstream file_bin(binFilepath, std::ios::binary);
  if (!file_bin.is_open()) {
    LOG(ERROR, "Failed to open binary file.");
  }
  std::vector<uint8_t> binaryCode((std::istreambuf_iterator<char>(file_bin)), std::istreambuf_iterator<char>());

  LOG(DEBUG, "========================================================");

  Cpu cpu(binaryCode);
  for (size_t i = 0; i < n_clock; ++i) {
    auto inst = cpu.fetch();
    if (inst.has_value()) {
      auto new_pc = cpu.execute(inst.value());
      if (new_pc.has_value()) {
        cpu.pc = new_pc.value();
        LOG(DEBUG, "Successfully executed instruction at clock cycle ", i + 1);
      } else {
        LOG(DEBUG, "Failed to execute instruction at clock cycle ", i + 1);
        break;
      }
    } else {
      LOG(DEBUG, "No more instructions to execute at clock cycle ", i + 1);
      break;
    }
    LOG(DEBUG, "========================================================");
  }
  return cpu;
}
}