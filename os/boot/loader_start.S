.section .text.init
.globl _boot_start

_boot_start:
    csrw mstatus, zero

    la sp, _boot_stack_top

    la t0, boot_trap
    csrw mtvec, t0

    la t0, _boot_bss_start
    la t1, _boot_bss_end
    bge t0, t1, 2f
1:
    sd zero, 0(t0)
    addi t0, t0, 8
    blt t0, t1, 1b
2:
    call boot_main

boot_trap:
    j boot_trap
