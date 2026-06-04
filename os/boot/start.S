.section .text.init
.globl _start
.globl trap_entry_m

_start:
    /* M 模式启动 */
    csrw mstatus, zero

    /* 设置 M 模式栈 */
    la sp, _stack_top

    /* M 模式 trap 向量：处理非委托陷阱 */
    la t0, trap_entry_m
    csrw mtvec, t0

    /* 清零 BSS 段 */
    la t0, _bss_start
    la t1, _bss_end
    bge t0, t1, 2f
1:
    sd zero, 0(t0)
    addi t0, t0, 8
    blt t0, t1, 1b
2:

    /*
     * 配置异常委托（medeleg）：
     *   bit 8: ECALL from U-mode → S-mode
     *   bit 9: ECALL from S-mode → S-mode
     */
    li t0, (1 << 8) | (1 << 9)
    csrw medeleg, t0

    /*
     * 配置中断委托（mideleg）：
     *   bit 5: Supervisor timer interrupt → S-mode
     */
    li t0, (1 << 5)
    csrw mideleg, t0

    /* 设置 S 模式 trap 向量 */
    la t0, trap_entry
    csrw stvec, t0

    /*
     * 切换到 S 模式运行 kernel_main：
     *   1. 设置 mstatus.MPP = Supervisor (01)
     *   2. 设置 mepc = kernel_main
     *   3. mret → 特权级 = MPP = S, PC = mepc = kernel_main
     */
    li t0, (1 << 11)           /* MPP = S (bit 11 = 1, bit 12 = 0) */
    csrs mstatus, t0

    la t0, kernel_main
    csrw mepc, t0

    mret

/*
 * M 模式 trap 入口（处理非委托异常/中断，例如未被 medeleg/mideleg 覆盖的陷阱）
 * 遇到非预期陷阱时进入死循环，方便调试
 */
trap_entry_m:
    j trap_entry_m

    /* 不应到达此处 */
3:
    j 3b
