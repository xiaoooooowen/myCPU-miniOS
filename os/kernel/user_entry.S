.section .user_text, "ax"
.align 4
.globl user_entry
.globl user_entry_end
user_entry:
    j shell_start

shell_prompt: .ascii "minios> "
shell_prompt_end:
newline: .ascii "\n"
newline_end:
erase_seq: .ascii "\b \b"
erase_seq_end:
clear_seq: .ascii "\033[2J\033[H"
clear_seq_end:
help_text:
    .ascii "help              show this help\n"
    .ascii "ls [path]         list files\n"
    .ascii "cd path           change directory\n"
    .ascii "pwd               print directory\n"
    .ascii "cat file          print file\n"
    .ascii "run program [&]   run built-in program\n"
    .ascii "ps                show processes\n"
    .ascii "kill PID          terminate process\n"
    .ascii "echo TEXT         print text\n"
    .ascii "clear             clear terminal\n"
    .ascii "exit              leave shell\n"
help_text_end:
unknown_text: .ascii "unknown command\n"
unknown_text_end:
missing_arg_text: .ascii "missing argument\n"
missing_arg_text_end:
operation_fail: .ascii "operation failed\n"
operation_fail_end:
run_fail_text: .ascii "run: program not found\n"
run_fail_text_end:
run_done_text: .ascii "run: child exited\n"
run_done_text_end:
bg_prefix: .ascii "[pid "
bg_prefix_end:
bg_suffix: .ascii "]\n"
bg_suffix_end:

cmd_help: .asciz "help"
cmd_ls: .asciz "ls"
cmd_cd: .asciz "cd"
cmd_pwd: .asciz "pwd"
cmd_cat: .asciz "cat"
cmd_run: .asciz "run"
cmd_ps: .asciz "ps"
cmd_kill: .asciz "kill"
cmd_echo: .asciz "echo"
cmd_clear: .asciz "clear"
cmd_exit: .asciz "exit"
    .align 4

shell_write:
    li a0, 1
    li a7, 64
    ecall
    ret

shell_write_range:
    sub a2, a2, a1
    j shell_write

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

/* Return argument pointer, or zero if the command word does not match. */
shell_match_word:
    mv t3, a0
    mv t4, a1
1:
    lbu t0, 0(t4)
    beq t0, zero, 2f
    lbu t1, 0(t3)
    bne t0, t1, 4f
    addi t3, t3, 1
    addi t4, t4, 1
    j 1b
2:
    lbu t0, 0(t3)
    beq t0, zero, 3f
    li t1, 32
    bne t0, t1, 4f
2:
    lbu t0, 0(t3)
    li t1, 32
    bne t0, t1, 3f
    addi t3, t3, 1
    j 2b
3:
    mv a0, t3
    ret
4:
    li a0, 0
    ret

shell_parse_uint:
    li t0, 0
    lbu t1, 0(a0)
    beq t1, zero, 3f
1:
    lbu t1, 0(a0)
    beq t1, zero, 2f
    li t2, '0'
    blt t1, t2, 3f
    li t2, '9'
    bgt t1, t2, 3f
    addi t1, t1, -48
    slli t2, t0, 3
    slli t3, t0, 1
    add t0, t2, t3
    add t0, t0, t1
    addi a0, a0, 1
    j 1b
2:
    mv a0, t0
    ret
3:
    li a0, -1
    ret

shell_print_pid:
    mv t0, a0
    li t1, 0
1:
    li t2, 10
    blt t0, t2, 2f
    addi t0, t0, -10
    addi t1, t1, 1
    j 1b
2:
    addi t0, t0, 48
    beq t1, zero, 3f
    addi t1, t1, 48
    sb t1, 128(sp)
    sb t0, 129(sp)
    addi a1, sp, 128
    li a2, 2
    j shell_write
3:
    sb t0, 128(sp)
    addi a1, sp, 128
    li a2, 1
    j shell_write

shell_start:
    addi sp, sp, -256

shell_prompt_loop:
1:
    li a0, -1
    li a1, 0
    li a2, 1
    li a7, 260
    ecall
    bgt a0, zero, 1b
    la a1, shell_prompt
    la a2, shell_prompt_end
    call shell_write_range
    li s0, 0
    mv s1, sp

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
    li t2, 127
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
    la a2, erase_seq_end
    call shell_write_range
    j shell_read_loop

shell_line_done:
    la a1, newline
    la a2, newline_end
    call shell_write_range
    add t0, s1, s0
    sb zero, 0(t0)
    mv s2, s1
1:
    lbu t0, 0(s2)
    li t1, 32
    bne t0, t1, shell_dispatch
    addi s2, s2, 1
    j 1b

shell_dispatch:
    beq t0, zero, shell_prompt_loop
    mv a0, s2
    la a1, cmd_help
    call shell_strcmp
    beq a0, zero, shell_cmd_help
    mv a0, s2
    la a1, cmd_pwd
    call shell_strcmp
    beq a0, zero, shell_cmd_pwd
    mv a0, s2
    la a1, cmd_ps
    call shell_strcmp
    beq a0, zero, shell_cmd_ps
    mv a0, s2
    la a1, cmd_clear
    call shell_strcmp
    beq a0, zero, shell_cmd_clear
    mv a0, s2
    la a1, cmd_exit
    call shell_strcmp
    beq a0, zero, shell_exit
    mv a0, s2
    la a1, cmd_ls
    call shell_match_word
    bne a0, zero, shell_cmd_ls
    mv a0, s2
    la a1, cmd_cd
    call shell_match_word
    bne a0, zero, shell_cmd_cd
    mv a0, s2
    la a1, cmd_cat
    call shell_match_word
    bne a0, zero, shell_cmd_cat
    mv a0, s2
    la a1, cmd_run
    call shell_match_word
    bne a0, zero, shell_cmd_run
    mv a0, s2
    la a1, cmd_kill
    call shell_match_word
    bne a0, zero, shell_cmd_kill
    mv a0, s2
    la a1, cmd_echo
    call shell_match_word
    bne a0, zero, shell_cmd_echo
    j shell_unknown

shell_cmd_help:
    la a1, help_text
    la a2, help_text_end
    call shell_write_range
    j shell_prompt_loop

shell_cmd_ls:
    lbu t0, 0(a0)
    bne t0, zero, 1f
    li a0, 0
1:
    li a7, 401
    ecall
    blt a0, zero, shell_failed
    j shell_prompt_loop

shell_cmd_cd:
    lbu t0, 0(a0)
    beq t0, zero, shell_missing_arg
    li a7, 402
    ecall
    blt a0, zero, shell_failed
    j shell_prompt_loop

shell_cmd_pwd:
    addi a0, sp, 128
    li a1, 128
    li a7, 403
    ecall
    blt a0, zero, shell_failed
    mv a2, a0
    addi a1, sp, 128
    call shell_write
    la a1, newline
    la a2, newline_end
    call shell_write_range
    j shell_prompt_loop

shell_cmd_cat:
    lbu t0, 0(a0)
    beq t0, zero, shell_missing_arg
    li a1, 0
    li a7, 56
    ecall
    blt a0, zero, shell_failed
    mv s3, a0
1:
    mv a0, s3
    addi a1, sp, 128
    li a2, 128
    li a7, 63
    ecall
    ble a0, zero, 2f
    mv a2, a0
    addi a1, sp, 128
    call shell_write
    j 1b
2:
    mv a0, s3
    li a7, 57
    ecall
    la a1, newline
    la a2, newline_end
    call shell_write_range
    j shell_prompt_loop

shell_cmd_run:
    mv s3, a0
    lbu t0, 0(s3)
    beq t0, zero, shell_missing_arg
    li s4, 0
    mv t1, s3
1:
    lbu t0, 0(t1)
    beq t0, zero, 2f
    addi t1, t1, 1
    j 1b
2:
    addi t1, t1, -1
3:
    blt t1, s3, 5f
    lbu t0, 0(t1)
    li t2, 32
    bne t0, t2, 4f
    sb zero, 0(t1)
    addi t1, t1, -1
    j 3b
4:
    li t2, '&'
    bne t0, t2, 5f
    li s4, 1
    sb zero, 0(t1)
3:
    addi t1, t1, -1
    blt t1, s3, 5f
    lbu t0, 0(t1)
    li t2, 32
    bne t0, t2, 5f
    sb zero, 0(t1)
    j 3b
5:
    li a7, 220
    ecall
    beq a0, zero, shell_run_child
    blt a0, zero, shell_failed
    mv s5, a0
    bne s4, zero, shell_run_background
shell_run_wait:
    mv a0, s5
    li a1, 0
    li a2, 0
    li a7, 260
    ecall
    li t0, -2
    beq a0, t0, shell_run_wait
    blt a0, zero, shell_failed
    la a1, run_done_text
    la a2, run_done_text_end
    call shell_write_range
    j shell_prompt_loop

shell_run_background:
    la a1, bg_prefix
    la a2, bg_prefix_end
    call shell_write_range
    mv a0, s5
    call shell_print_pid
    la a1, bg_suffix
    la a2, bg_suffix_end
    call shell_write_range
    j shell_prompt_loop

shell_run_child:
    mv a0, s3
    li a7, 404
    ecall
    la a1, run_fail_text
    la a2, run_fail_text_end
    call shell_write_range
    li a0, 127
    li a7, 93
    ecall
    j .

shell_cmd_kill:
    lbu t0, 0(a0)
    beq t0, zero, shell_missing_arg
    call shell_parse_uint
    blt a0, zero, shell_failed
    li a7, 405
    ecall
    blt a0, zero, shell_failed
    j shell_prompt_loop

shell_cmd_echo:
    mv s3, a0
    mv t0, s3
1:
    lbu t1, 0(t0)
    beq t1, zero, 2f
    addi t0, t0, 1
    j 1b
2:
    mv a1, s3
    sub a2, t0, s3
    call shell_write
    la a1, newline
    la a2, newline_end
    call shell_write_range
    j shell_prompt_loop

shell_cmd_ps:
    li a7, 400
    ecall
    j shell_prompt_loop

shell_cmd_clear:
    la a1, clear_seq
    la a2, clear_seq_end
    call shell_write_range
    j shell_prompt_loop

shell_missing_arg:
    la a1, missing_arg_text
    la a2, missing_arg_text_end
    call shell_write_range
    j shell_prompt_loop

shell_failed:
    la a1, operation_fail
    la a2, operation_fail_end
    call shell_write_range
    j shell_prompt_loop

shell_unknown:
    la a1, unknown_text
    la a2, unknown_text_end
    call shell_write_range
    j shell_prompt_loop

shell_exit:
    li a0, 0
    li a7, 93
    ecall
    j .

user_entry_end:

.align 4
.globl user_spin
.globl user_spin_end
user_spin:
    j spin_start
spin_msg: .ascii "[spin] running; use ps and kill PID\n"
spin_msg_end:
spin_start:
    li a0, 1
    la a1, spin_msg
    la a2, spin_msg_end
    sub a2, a2, a1
    li a7, 64
    ecall
    li t0, 0
1:
    addi t0, t0, 1
    xor t1, t1, t0
    j 1b
user_spin_end:

.align 4
.globl user_fstest
.globl user_fstest_end
user_fstest:
    j fstest_start
fstest_path: .asciz "/tmp/fstest.txt"
fstest_data: .ascii "MiniFS persists across reboots.\n"
fstest_data_end:
fstest_pass: .ascii "[fstest] PASS\n"
fstest_pass_end:
fstest_fail: .ascii "[fstest] FAIL\n"
fstest_fail_end:
    .align 4
fstest_start:
    addi sp, sp, -64
    la a0, fstest_path
    li a1, 3
    li a7, 56
    ecall
    blt a0, zero, fstest_failed
    mv s0, a0
    mv a0, s0
    la a1, fstest_data
    la a2, fstest_data_end
    sub a2, a2, a1
    li a7, 64
    ecall
    blt a0, zero, fstest_failed
    mv a0, s0
    li a7, 57
    ecall
    la a0, fstest_path
    li a1, 0
    li a7, 56
    ecall
    blt a0, zero, fstest_failed
    mv s0, a0
    mv a0, s0
    mv a1, sp
    li a2, 32
    li a7, 63
    ecall
    la t0, fstest_data_end
    la t1, fstest_data
    sub t0, t0, t1
    bne a0, t0, fstest_failed
    li t2, 0
1:
    beq t2, t0, fstest_ok
    add t3, sp, t2
    lbu t4, 0(t3)
    add t3, t1, t2
    lbu t5, 0(t3)
    bne t4, t5, fstest_failed
    addi t2, t2, 1
    j 1b
fstest_ok:
    la a1, fstest_pass
    la a2, fstest_pass_end
    li a0, 1
    sub a2, a2, a1
    li a7, 64
    ecall
    li a0, 0
    li a7, 93
    ecall
fstest_failed:
    la a1, fstest_fail
    la a2, fstest_fail_end
    li a0, 1
    sub a2, a2, a1
    li a7, 64
    ecall
    li a0, 1
    li a7, 93
    ecall
    j .
user_fstest_end:

.align 4
.globl user_forktest
.globl user_forktest_end
user_forktest:
    j forktest_start
forktest_pass: .ascii "[forktest] PASS\n"
forktest_pass_end:
forktest_fail: .ascii "[forktest] FAIL\n"
forktest_fail_end:
    .align 4
forktest_start:
    addi sp, sp, -32
    li a7, 220
    ecall
    beq a0, zero, forktest_child
    blt a0, zero, forktest_failed
    mv s0, a0
forktest_wait:
    mv a0, s0
    mv a1, sp
    li a2, 0
    li a7, 260
    ecall
    li t0, -2
    beq a0, t0, forktest_wait
    blt a0, zero, forktest_failed
    lw t0, 0(sp)
    li t1, 42
    bne t0, t1, forktest_failed
    la a1, forktest_pass
    la a2, forktest_pass_end
    j forktest_print
forktest_child:
    li a0, 4
    li a7, 221
    ecall
forktest_failed:
    la a1, forktest_fail
    la a2, forktest_fail_end
forktest_print:
    li a0, 1
    sub a2, a2, a1
    li a7, 64
    ecall
    li a0, 0
    li a7, 93
    ecall
    j .
user_forktest_end:

.align 4
.globl user_fork_child
.globl user_fork_child_end
user_fork_child:
    li a0, 42
    li a7, 93
    ecall
    j .
user_fork_child_end:
