/*
 * switch_to(struct context *prev, struct context *next)
 *   a0 = prev
 *   a1 = next
 *
 * 保存当前 callee-saved 寄存器到 prev，
 * 从 next 恢复 callee-saved 寄存器并返回。
 *
 * Callee-saved: ra, sp, s0-s11 (共 14 个寄存器)
 */
.section .text
.globl switch_to
.align 4
switch_to:
    /* 保存当前上下文到 prev (a0) */
    sd ra,   0(a0)
    sd sp,   8(a0)
    sd s0,  16(a0)
    sd s1,  24(a0)
    sd s2,  32(a0)
    sd s3,  40(a0)
    sd s4,  48(a0)
    sd s5,  56(a0)
    sd s6,  64(a0)
    sd s7,  72(a0)
    sd s8,  80(a0)
    sd s9,  88(a0)
    sd s10, 96(a0)
    sd s11,104(a0)

    /* 从 next (a1) 恢复上下文 */
    ld ra,   0(a1)
    ld sp,   8(a1)
    ld s0,  16(a1)
    ld s1,  24(a1)
    ld s2,  32(a1)
    ld s3,  40(a1)
    ld s4,  48(a1)
    ld s5,  56(a1)
    ld s6,  64(a1)
    ld s7,  72(a1)
    ld s8,  80(a1)
    ld s9,  88(a1)
    ld s10, 96(a1)
    ld s11,104(a1)

    ret
