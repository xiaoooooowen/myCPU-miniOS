#ifndef MINIOS_USER_H
#define MINIOS_USER_H

#include <stdint.h>
#include "task.h"

#define USER_TEXT_VA   0x10000ULL
#define USER_STACK_VA  0x20000ULL
#define USER_STACK_TOP (USER_STACK_VA + 0x1000ULL)

/*
 * 用户模式初始化：
 *   1. 分配物理页存放用户程序代码和用户栈
 *   2. 将用户程序 .user_text 段复制到用户代码页
 *   3. 构建用户页表映射（VA 0x10000 → 代码页, 0x20000 → 栈页）
 *   4. 保留 TEST_FINISH (0x100000) 内核映射
 */
void user_init(void);
int user_space_create(struct task_address_space *space, int image_id);
int user_space_clone(struct task_address_space *dst,
                     const struct task_address_space *src);
void user_space_destroy(struct task_address_space *space);
int user_exec(uint64_t *trap_frame, int image_id);

/*
 * 通过 sret 从 S 模式切换到 U 模式：
 *   设置 sepc = 0x10000（用户入口）
 *   设置 sstatus.SPP = 0（U 模式）
 *   设置 sp = 0x21000（用户栈顶）
 *   执行 sret
 *
 * 注意：此函数不会返回。
 */
void enter_user(void);

#endif /* MINIOS_USER_H */
