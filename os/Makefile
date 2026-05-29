CROSS_COMPILE ?= riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

CFLAGS = -march=rv64i_zicsr -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -nostartfiles -O0 -g
LDFLAGS = -T linker.ld -nostdlib

SRC_DIR = .
BUILD_DIR = build

BOOT_SRC = boot/start.S
KERNEL_SRC = kernel/kernel.c

OBJS = $(BUILD_DIR)/start.o $(BUILD_DIR)/kernel.o

.PHONY: all clean dump

all: $(BUILD_DIR) $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/kernel.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/start.o: $(BOOT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.o: $(KERNEL_SRC) | $(BUILD_DIR)
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
