CROSS_COMPILE ?= riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

BOOT_DIAGNOSTICS ?= 0
BOOT_COLOR ?= 1

CFLAGS = -march=rv64i_zicsr -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -nostartfiles -O0 -g -DMINIOS_USE_S_MODE
CFLAGS += -DMINIOS_BOOT_DIAGNOSTICS=$(BOOT_DIAGNOSTICS)
CFLAGS += -DMINIOS_BOOT_COLOR=$(BOOT_COLOR)
CFLAGS += -Iplatform/include
LDFLAGS = -T linker.ld -nostdlib
LOADER_CFLAGS = -march=rv64i_zicsr -mabi=lp64 -mcmodel=medany \
	-ffreestanding -fno-builtin -fno-pic -msmall-data-limit=0 \
	-nostdlib -nostartfiles -O2
LOADER_LDFLAGS = -T boot/loader.ld -nostdlib -Wl,--no-relax,--build-id=none
USER_CFLAGS = -march=rv64i_zicsr -mabi=lp64 -mcmodel=medany \
	-ffreestanding -fno-builtin -fno-pic -msmall-data-limit=0 \
	-nostdlib -nostartfiles -Os -Iuser/include
USER_CFLAGS += -DMINIOS_BOOT_COLOR=$(BOOT_COLOR)
USER_LDFLAGS = -T user/user.ld -nostdlib -nostartfiles -static \
	-Wl,--no-relax,--build-id=none,-s

SRC_DIR = .
BUILD_DIR = build
USER_BUILD_DIR = $(BUILD_DIR)/user
ROOTFS_DIR = $(BUILD_DIR)/rootfs
DISK_IMAGE = disk.img
CEMU ?= ../build_wsl/cemu

KERNEL_ENTRY_SRC = boot/start.S
KERNEL_SRCS = kernel/kernel.c kernel/uart.c kernel/printk.c kernel/trap.c kernel/timer.c kernel/mem.c kernel/task.c kernel/sync.c kernel/syscall.c kernel/vm.c kernel/user.c kernel/ramfs.c kernel/block.c kernel/minifs.c kernel/string.c platform/cemu/platform_uart.c
KERNEL_ASMS = kernel/trap.S kernel/switch.S

OBJS = $(BUILD_DIR)/start.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/uart.o $(BUILD_DIR)/printk.o $(BUILD_DIR)/trap_handler.o $(BUILD_DIR)/trap_entry.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/mem.o $(BUILD_DIR)/task.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/switch.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/vm.o $(BUILD_DIR)/user.o $(BUILD_DIR)/ramfs.o $(BUILD_DIR)/block.o $(BUILD_DIR)/minifs.o $(BUILD_DIR)/string.o $(BUILD_DIR)/platform_uart.o
LOADER_OBJS = $(BUILD_DIR)/loader_start.o $(BUILD_DIR)/loader.o

BIN_PROGRAMS = shell ls cat echo pwd ps kill env mkdir rm touch write
TEST_PROGRAMS = spin fstest forktest argtest testall
USER_COMMON = $(USER_BUILD_DIR)/crt0.o $(USER_BUILD_DIR)/libc.o
BIN_TARGETS = $(addprefix $(ROOTFS_DIR)/bin/,$(BIN_PROGRAMS))
TEST_TARGETS = $(addprefix $(ROOTFS_DIR)/tests/,$(TEST_PROGRAMS))

.PHONY: all users disk install-kernel run clean distclean dump
.SECONDARY: $(addprefix $(USER_BUILD_DIR)/,$(addsuffix .o,$(BIN_PROGRAMS) $(TEST_PROGRAMS)))

all: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin users

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/loader_start.o: boot/loader_start.S | $(BUILD_DIR)
	$(CC) $(LOADER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/loader.o: boot/loader.c | $(BUILD_DIR)
	$(CC) $(LOADER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.elf: $(LOADER_OBJS) boot/loader.ld
	$(CC) $(LOADER_CFLAGS) $(LOADER_LDFLAGS) $(LOADER_OBJS) -lgcc -o $@

$(BUILD_DIR)/boot.bin: $(BUILD_DIR)/boot.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/start.o: $(KERNEL_ENTRY_SRC) | $(BUILD_DIR)
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

$(BUILD_DIR)/sync.o: kernel/sync.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/switch.o: kernel/switch.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: kernel/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vm.o: kernel/vm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user.o: kernel/user.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ramfs.o: kernel/ramfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/block.o: kernel/block.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/minifs.o: kernel/minifs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: kernel/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/platform_uart.o: platform/cemu/platform_uart.c platform/include/platform.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -lgcc -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(USER_BUILD_DIR) $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/tests:
	mkdir -p $@

$(USER_BUILD_DIR)/crt0.o: user/crt0.S | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD_DIR)/libc.o: user/libc.c user/include/user.h | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD_DIR)/%.o: user/%.c user/include/user.h | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(ROOTFS_DIR)/bin/%: $(USER_BUILD_DIR)/%.o $(USER_COMMON) user/user.ld | $(ROOTFS_DIR)/bin
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(USER_COMMON) $< -lgcc -o $@

$(ROOTFS_DIR)/tests/%: $(USER_BUILD_DIR)/%.o $(USER_COMMON) user/user.ld | $(ROOTFS_DIR)/tests
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(USER_COMMON) $< -lgcc -o $@

$(ROOTFS_DIR)/README: user/README | $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/tests
	cp $< $@

$(ROOTFS_DIR)/tmp: | $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/tests
	mkdir -p $@

users: $(BIN_TARGETS) $(TEST_TARGETS) $(ROOTFS_DIR)/README $(ROOTFS_DIR)/tmp

disk: users $(BUILD_DIR)/kernel.bin
	python3 ../tools/mkfs_minifs.py $(ROOTFS_DIR) $(DISK_IMAGE) $(if $(filter 1,$(FORCE)),--force,)
	python3 ../tools/install_kernel.py $(DISK_IMAGE) $(BUILD_DIR)/kernel.bin

install-kernel: $(BUILD_DIR)/kernel.bin
	@test -f $(DISK_IMAGE) || { echo "missing $(DISK_IMAGE); run make disk"; exit 1; }
	python3 ../tools/install_kernel.py $(DISK_IMAGE) $(BUILD_DIR)/kernel.bin

run: all
	@if [ ! -f $(DISK_IMAGE) ]; then $(MAKE) disk; else $(MAKE) install-kernel; fi
	$(CEMU) $(BUILD_DIR)/boot.bin --disk $(DISK_IMAGE)

dump: $(BUILD_DIR)/boot.elf $(BUILD_DIR)/kernel.elf
	$(OBJDUMP) -h $(BUILD_DIR)/boot.elf
	@echo ""
	$(OBJDUMP) -d $(BUILD_DIR)/boot.elf
	@echo ""
	$(OBJDUMP) -h $(BUILD_DIR)/kernel.elf
	@echo ""
	$(OBJDUMP) -d $(BUILD_DIR)/kernel.elf

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -f $(DISK_IMAGE)
