.section .text.init
.globl _start

_start:
    csrw mstatus, zero

    la sp, _stack_top

    la t0, _bss_start
    la t1, _bss_end
    bge t0, t1, 2f
1:
    sd zero, 0(t0)
    addi t0, t0, 8
    blt t0, t1, 1b
2:

    call kernel_main

3:
    j 3b
