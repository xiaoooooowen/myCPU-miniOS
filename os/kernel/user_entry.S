/*
 * 两个位置无关的内置用户程序：
 *   image 0: fork -> parent waitpid / child exec
 *   image 1: 输出消息后 exit(7)
 */

.section .user_text, "ax"
.align 4
.globl user_entry
.globl user_entry_end
user_entry:
    j parent_start

parent_msg:
    .ascii "[user parent] forked child\n"
parent_msg_end:
parent_done_msg:
    .ascii "[user parent] waitpid complete\n"
parent_done_msg_end:
fork_fail_msg:
    .ascii "[user] fork/exec failed\n"
fork_fail_msg_end:
    .align 4

parent_start:
    li a7, 220                 /* SYS_FORK */
    ecall
    beq a0, zero, child_path
    blt a0, zero, failure_path

    mv s0, a0                 /* 保存 child pid */
    li a7, 64
    li a0, 1
    la a1, parent_msg
    li a2, 27
    ecall

    li a7, 260                /* waitpid(child, NULL, 0) */
    mv a0, s0
    li a1, 0
    li a2, 0
    ecall

    li a7, 64
    li a0, 1
    la a1, parent_done_msg
    li a2, 31
    ecall

    li a7, 93
    li a0, 0
    ecall

child_path:
    li a7, 221                /* exec(image 1) */
    li a0, 1
    ecall

failure_path:
    li a7, 64
    li a0, 1
    la a1, fork_fail_msg
    li a2, 24
    ecall
    li a7, 93
    li a0, 1
    ecall
    j .

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
