.section .text.start
.globl _start
_start:
    ld a0, 0(sp)
    addi a1, sp, 8
    slli t0, a0, 3
    add t0, a1, t0
    addi a2, t0, 8
    call main
    li a7, 93
    ecall
1:
    j 1b
