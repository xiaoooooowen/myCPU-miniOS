CROSS_COMPILE ?= riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

CFLAGS = -march=rv64i_zicsr -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -nostartfiles -O0 -g -DMINIOS_USE_S_MODE
LDFLAGS = -T linker.ld -nostdlib

SRC_DIR = .
BUILD_DIR = build

BOOT_SRC = boot/start.S
KERNEL_SRCS = kernel/kernel.c kernel/uart.c kernel/printk.c kernel/trap.c kernel/timer.c kernel/mem.c kernel/task.c kernel/syscall.c kernel/vm.c kernel/user.c
KERNEL_ASMS = kernel/trap.S kernel/switch.S kernel/user_entry.S

OBJS = $(BUILD_DIR)/start.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/uart.o $(BUILD_DIR)/printk.o $(BUILD_DIR)/trap_handler.o $(BUILD_DIR)/trap_entry.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/mem.o $(BUILD_DIR)/task.o $(BUILD_DIR)/switch.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/vm.o $(BUILD_DIR)/user.o $(BUILD_DIR)/user_entry.o

.PHONY: all clean dump

all: $(BUILD_DIR) $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/kernel.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/start.o: $(BOOT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uart.o: kernel/uart.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/printk.o: kernel/printk.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trap_handler.o: kernel/trap.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trap_entry.o: kernel/trap.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: kernel/timer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mem.o: kernel/mem.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/task.o: kernel/task.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/switch.o: kernel/switch.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: kernel/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vm.o: kernel/vm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user.o: kernel/user.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user_entry.o: kernel/user_entry.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

dump: $(BUILD_DIR)/kernel.elf
	$(OBJDUMP) -h $(BUILD_DIR)/kernel.elf
	@echo ""
	$(OBJDUMP) -d $(BUILD_DIR)/kernel.elf

clean:
	rm -rf $(BUILD_DIR)
