/*
 * 两个位置无关的内置用户程序：
 *   image 0: 交互式 MiniOS shell
 *   image 1: 由 shell 的 run 命令通过 fork/exec 启动
 */

.section .user_text, "ax"
.align 4
.globl user_entry
.globl user_entry_end
user_entry:
    j shell_start

shell_banner:
    .ascii "\nMiniOS shell\nType 'help' for commands.\n"
shell_banner_end:
shell_prompt:
    .ascii "minios> "
shell_prompt_end:
help_text:
    .ascii "help        show this help\n"
    .ascii "echo TEXT   print TEXT\n"
    .ascii "ps          show processes\n"
    .ascii "run         run fork/exec demo\n"
    .ascii "clear       clear the terminal\n"
    .ascii "exit        leave the shell\n"
help_text_end:
unknown_text:
    .ascii "unknown command\n"
unknown_text_end:
run_fail_text:
    .ascii "run: fork/exec failed\n"
run_fail_text_end:
run_done_text:
    .ascii "run: child exited\n"
run_done_text_end:
newline:
    .ascii "\n"
newline_end:
erase_seq:
    .ascii "\b \b"
erase_seq_end:
clear_seq:
    .ascii "\033[2J\033[H"
clear_seq_end:
    .align 4

/* write(1, a1, a2) */
shell_write:
    li a0, 1
    li a7, 64
    ecall
    ret

/* strcmp(a0, a1): returns zero when equal. */
shell_strcmp:
1:
    lbu t0, 0(a0)
    lbu t1, 0(a1)
    bne t0, t1, 2f
    beq t0, zero, 3f
    addi a0, a0, 1
    addi a1, a1, 1
    j 1b
2:
    li a0, 1
    ret
3:
    li a0, 0
    ret

shell_start:
    addi sp, sp, -128          /* 64-byte line buffer plus scratch space */
    la a1, shell_banner
    li a2, 40
    call shell_write

shell_prompt_loop:
    la a1, shell_prompt
    li a2, 8
    call shell_write
    li s0, 0                   /* current line length */
    mv s1, sp                  /* line buffer */

shell_read_loop:
    add t0, s1, s0
    li a0, 0
    mv a1, t0
    li a2, 1
    li a7, 63
    ecall
    ble a0, zero, shell_exit
    add t0, s1, s0
    lbu t1, 0(t0)
    li t2, 10
    beq t1, t2, shell_line_done
    li t2, 13
    beq t1, t2, shell_line_done
    li t2, 8
    beq t1, t2, shell_backspace
    li t2, 127
    beq t1, t2, shell_backspace
    li t2, 63
    bge s0, t2, shell_read_loop

    mv a1, t0
    li a2, 1
    call shell_write
    addi s0, s0, 1
    j shell_read_loop

shell_backspace:
    beq s0, zero, shell_read_loop
    addi s0, s0, -1
    la a1, erase_seq
    li a2, 3
    call shell_write
    j shell_read_loop

shell_line_done:
    la a1, newline
    li a2, 1
    call shell_write
    add t0, s1, s0
    sb zero, 0(t0)

    mv s2, s1
shell_skip_spaces:
    lbu t0, 0(s2)
    li t1, 32
    bne t0, t1, shell_dispatch
    addi s2, s2, 1
    j shell_skip_spaces

shell_dispatch:
    beq t0, zero, shell_prompt_loop

    mv a0, s2
    la a1, cmd_help
    call shell_strcmp
    beq a0, zero, shell_cmd_help

    mv a0, s2
    la a1, cmd_ps
    call shell_strcmp
    beq a0, zero, shell_cmd_ps

    mv a0, s2
    la a1, cmd_run
    call shell_strcmp
    beq a0, zero, shell_cmd_run

    mv a0, s2
    la a1, cmd_clear
    call shell_strcmp
    beq a0, zero, shell_cmd_clear

    mv a0, s2
    la a1, cmd_exit
    call shell_strcmp
    beq a0, zero, shell_exit

    lbu t0, 0(s2)
    li t1, 'e'
    bne t0, t1, shell_unknown
    lbu t0, 1(s2)
    li t1, 'c'
    bne t0, t1, shell_unknown
    lbu t0, 2(s2)
    li t1, 'h'
    bne t0, t1, shell_unknown
    lbu t0, 3(s2)
    li t1, 'o'
    bne t0, t1, shell_unknown
    lbu t0, 4(s2)
    beq t0, zero, shell_cmd_echo_empty
    li t1, 32
    bne t0, t1, shell_unknown

    addi s3, s2, 5
shell_echo_skip:
    lbu t0, 0(s3)
    bne t0, t1, shell_echo_measure
    addi s3, s3, 1
    j shell_echo_skip
shell_echo_measure:
    mv t2, s3
shell_echo_scan:
    lbu t0, 0(t2)
    beq t0, zero, shell_echo_write
    addi t2, t2, 1
    j shell_echo_scan
shell_echo_write:
    mv a1, s3
    sub a2, t2, s3
    call shell_write
shell_cmd_echo_empty:
    la a1, newline
    li a2, 1
    call shell_write
    j shell_prompt_loop

shell_cmd_help:
    la a1, help_text
    li a2, 167
    call shell_write
    j shell_prompt_loop

shell_cmd_ps:
    li a7, 400
    ecall
    j shell_prompt_loop

shell_cmd_clear:
    la a1, clear_seq
    li a2, 7
    call shell_write
    j shell_prompt_loop

shell_cmd_run:
    li a7, 220                 /* fork */
    ecall
    beq a0, zero, shell_run_child
    blt a0, zero, shell_run_fail
    mv s3, a0
shell_run_wait:
    mv a0, s3
    li a1, 0
    li a2, 0
    li a7, 260                 /* waitpid */
    ecall
    blt a0, zero, shell_run_wait
    la a1, run_done_text
    li a2, 18
    call shell_write
    j shell_prompt_loop

shell_run_child:
    li a0, 1
    li a7, 221                 /* exec image 1 */
    ecall
shell_run_fail:
    la a1, run_fail_text
    li a2, 22
    call shell_write
    j shell_prompt_loop

shell_unknown:
    la a1, unknown_text
    li a2, 16
    call shell_write
    j shell_prompt_loop

shell_exit:
    li a0, 0
    li a7, 93
    ecall
    j .

cmd_help:  .asciz "help"
cmd_ps:    .asciz "ps"
cmd_run:   .asciz "run"
cmd_clear: .asciz "clear"
cmd_exit:  .asciz "exit"
    .byte 0

user_entry_end:

.align 4
.globl user_worker
.globl user_worker_end
user_worker:
    j worker_start
worker_msg:
    .ascii "[user child] exec image running\n"
worker_msg_end:
    .align 4
worker_start:
    li a7, 64
    li a0, 1
    la a1, worker_msg
    li a2, 32
    ecall

    li a7, 93
    li a0, 7
    ecall
    j .
user_worker_end:
