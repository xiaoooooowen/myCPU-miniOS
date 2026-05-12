#ifndef GENERATE_RV_H
#define GENERATE_RV_H

#include <gtest/gtest.h>
#include <fstream>
#include "../src/cup.h"
namespace cemu {

// 消除警告： warning: cannot find entry symbol _start; defaulting to 0000000000000000
// 增加 .option norvc 禁用 C（压缩指令）扩展，确保全部生成 32-位 指令
const std::string start = ".option norvc \n.global _start \n _start:\n";

void generate_rv_assembly(const std::string& c_src);
void generate_rv_obj(const std::string& assembly);
void generate_rv_binary(const std::string& obj);
Cpu rv_helper(const std::string& code, const std::string& testname, size_t n_clock);
}
#endif  // GENERATE_RV_H
