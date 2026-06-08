/*
 * user_entry.S — 用户程序入口
 *
 * 编译到 .user_text 段，由 user_init() 复制到用户虚拟地址 0x10000。
 * 所有指令和 label 使用 PC 相对寻址，确保位置无关。
 *
 * 功能：
 *   1. sys_write(1, msg, 17) — 通过 ecall 输出 "Hello from user!"
 *   2. sys_exit(0)           — 通过 ecall 停机
 */

.section .user_text, "ax"
.globl user_entry
.align 4
user_entry:
    j       2f
1:
    .string "Hello from user!\n"
    .align 4
2:
    # sys_write(1, msg, 17)
    li      a7, 64              # SYS_WRITE
    li      a0, 1               # fd = stdout
    la      a1, 1b              # buf = msg (PC-relative, 位置无关)
    li      a2, 17              # len = 17
    ecall

    # sys_exit(0)
    li      a7, 93              # SYS_EXIT
    li      a0, 0               # code = 0
    ecall

    # 不应到达此处
    j       .
