.section .text
.globl trap_entry
.align 4
trap_entry:
    # 分配 trap 帧空间：32 * 8 = 256 字节
    addi sp, sp, -256

    # 先保存 t0（进入 trap 前的原始值），再借用 t0 计算原始 sp
    sd t0,  40(sp)   # x5 (t0) — 先保存原始值
    addi t0, sp, 256  # t0 = 进入 trap 前的 sp
    sd t0,  16(sp)   # x2 (sp) — 保存原始 sp
    ld t0,  40(sp)   # 恢复 t0 原始值

    # 保存其余通用寄存器（x0 硬连线零，跳过）
    sd x1,   8(sp)   # ra
    sd x3,  24(sp)   # gp
    sd x4,  32(sp)   # tp
    sd x6,  48(sp)   # t1
    sd x7,  56(sp)   # t2
    sd x8,  64(sp)   # s0/fp
    sd x9,  72(sp)   # s1
    sd x10, 80(sp)   # a0
    sd x11, 88(sp)   # a1
    sd x12, 96(sp)   # a2
    sd x13,104(sp)   # a3
    sd x14,112(sp)   # a4
    sd x15,120(sp)   # a5
    sd x16,128(sp)   # a6
    sd x17,136(sp)   # a7
    sd x18,144(sp)   # s2
    sd x19,152(sp)   # s3
    sd x20,160(sp)   # s4
    sd x21,168(sp)   # s5
    sd x22,176(sp)   # s6
    sd x23,184(sp)   # s7
    sd x24,192(sp)   # s8
    sd x25,200(sp)   # s9
    sd x26,208(sp)   # s10
    sd x27,216(sp)   # s11
    sd x28,224(sp)   # t3
    sd x29,232(sp)   # t4
    sd x30,240(sp)   # t5
    sd x31,248(sp)   # t6

    # 调用 C 语言 trap 处理函数，传递 trap frame 基址
    mv a0, sp
    call trap_handler

    # 恢复所有寄存器（sp 必须最后恢复）
    ld x1,   8(sp)   # ra
    ld x3,  24(sp)   # gp
    ld x4,  32(sp)   # tp
    ld x5,  40(sp)   # t0
    ld x6,  48(sp)   # t1
    ld x7,  56(sp)   # t2
    ld x8,  64(sp)   # s0/fp
    ld x9,  72(sp)   # s1
    ld x10, 80(sp)   # a0
    ld x11, 88(sp)   # a1
    ld x12, 96(sp)   # a2
    ld x13,104(sp)   # a3
    ld x14,112(sp)   # a4
    ld x15,120(sp)   # a5
    ld x16,128(sp)   # a6
    ld x17,136(sp)   # a7
    ld x18,144(sp)   # s2
    ld x19,152(sp)   # s3
    ld x20,160(sp)   # s4
    ld x21,168(sp)   # s5
    ld x22,176(sp)   # s6
    ld x23,184(sp)   # s7
    ld x24,192(sp)   # s8
    ld x25,200(sp)   # s9
    ld x26,208(sp)   # s10
    ld x27,216(sp)   # s11
    ld x28,224(sp)   # t3
    ld x29,232(sp)   # t4
    ld x30,240(sp)   # t5
    ld x31,248(sp)   # t6
    ld sp,  16(sp)   # sp 最后恢复

    sret
