cmake_minimum_required(VERSION 3.27)
project(cemu LANGUAGES CXX)

option(CEMU_TRACE "Enable per-instruction simulator trace logging" OFF)

# 设置C++标准为C++23
set(CMAKE_CXX_STANDARD 23)

# 公共部分
set(COMMON_SOURCES
        cemu/src/param.h
        cemu/src/dram.h
        cemu/src/dram.cpp
        cemu/src/bus.h
        cemu/src/bus.cpp
        cemu/src/cpu.h
        cemu/src/cpu.cpp
        cemu/src/instructions.h
        cemu/src/instructions.cpp
        cemu/src/log.h
        cemu/src/mmu.h
        cemu/src/mmu.cpp
        cemu/src/csr.h
        cemu/src/csr.cpp
        cemu/src/exception.cpp
        cemu/src/exception.h
        cemu/src/plic.cpp
        cemu/src/plic.h
        cemu/src/clint.cpp
        cemu/src/clint.h
        cemu/src/uart.cpp
        cemu/src/uart.h
        cemu/src/block_device.cpp
        cemu/src/block_device.h
)

add_library(common_library ${COMMON_SOURCES})
if(CEMU_TRACE)
    target_compile_definitions(common_library PUBLIC CEMU_TRACE=1)
endif()

# 可执行文件
add_executable(cemu
        cemu/src/main.cpp
)

# 将库链接到 cemu 可执行文件
target_link_libraries(cemu common_library)

# 启用测试并添加测试文件
enable_testing()
add_subdirectory("third_party/googletest")

add_executable(unit_test
        tests/cemu/unit/unitest/instructions_test.cpp
        tests/cemu/unit/test_util.h
        tests/cemu/unit/test_util.cpp
        tests/cemu/unit/unitest/dram_test.cpp
        tests/cemu/unit/unitest/bus_test.cpp
        tests/cemu/unit/unitest/cpu_test.cpp
        tests/cemu/unit/unitest/csr_test.cpp
        tests/cemu/unit/unitest/exception.cpp
        tests/cemu/unit/unitest/plic_test.cpp
        tests/cemu/unit/unitest/client_test.cpp
        tests/cemu/unit/unitest/uart_test.cpp
        tests/cemu/unit/unitest/mmu_test.cpp
)

# 将库链接到 unit_test 可执行文件
target_link_libraries(unit_test common_library GTest::gtest_main)

# 配置 GoogleTest
include(GoogleTest)
gtest_discover_tests(unit_test)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME mkfs_minifs
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/tools/test_mkfs_minifs.py
    )
    add_test(
        NAME install_kernel
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/tools/test_install_kernel.py
    )
    add_test(
        NAME minios_integration
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/os/integration/test_minios_integration.py
    )
    set_tests_properties(minios_integration PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
        LABELS "integration;minios"
    )
endif()
