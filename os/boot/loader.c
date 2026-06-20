#include <stdint.h>

#define UART_BASE       0x10000000UL
#define UART_THR        0
#define UART_LSR        5
#define UART_LSR_EMPTY  (1U << 5)

#define BLOCK_BASE       0x10001000UL
#define BLOCK_REG_SECTOR (BLOCK_BASE + 0x00)
#define BLOCK_REG_CMD    (BLOCK_BASE + 0x08)
#define BLOCK_REG_STATUS (BLOCK_BASE + 0x0c)
#define BLOCK_DATA       (BLOCK_BASE + 0x100)
#define BLOCK_CMD_READ   1U
#define BLOCK_SECTOR_SIZE 512U

#define KERNEL_SLOT_START 15360U
#define KERNEL_SLOT_SECTORS 1024U
#define KERNEL_LOAD_ADDRESS 0x80200000ULL
#define KERNEL_SLOT_PAYLOAD_MAX \
    ((KERNEL_SLOT_SECTORS - 1U) * BLOCK_SECTOR_SIZE)

#define BOOT_IMAGE_VERSION 1U
#define BOOT_IMAGE_HEADER_SIZE 64U

struct boot_image_header {
    uint8_t magic[8];
    uint32_t version;
    uint32_t header_size;
    uint64_t load_address;
    uint64_t entry;
    uint32_t image_size;
    uint32_t checksum;
    uint8_t reserved[24];
};

_Static_assert(sizeof(struct boot_image_header) == BOOT_IMAGE_HEADER_SIZE, "boot image header layout changed");
static uint8_t sector_buffer[BLOCK_SECTOR_SIZE]
    __attribute__((aligned(8)));

static void uart_putc(char character) {
    volatile uint8_t *uart = (volatile uint8_t *)UART_BASE;
    while ((uart[UART_LSR] & UART_LSR_EMPTY) == 0)
        ;
    uart[UART_THR] = (uint8_t)character;
}

static void uart_puts(const char *string) {
    while (*string != '\0')
        uart_putc(*string++);
}

static void uart_put_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc(digits[(value >> shift) & 0xf]);
}

static int bytes_equal(const uint8_t *left, const uint8_t *right,
                       uint32_t length) {
    for (uint32_t i = 0; i < length; i++)
        if (left[i] != right[i])
            return 0;
    return 1;
}

static int block_read(uint32_t sector, uint8_t *buffer) {
    *(volatile uint64_t *)BLOCK_REG_SECTOR = sector;
    *(volatile uint32_t *)BLOCK_REG_CMD = BLOCK_CMD_READ;
    if (*(volatile uint32_t *)BLOCK_REG_STATUS != 0)
        return -1;

    volatile uint64_t *source = (volatile uint64_t *)BLOCK_DATA;
    uint64_t *destination = (uint64_t *)buffer;
    for (uint32_t i = 0; i < BLOCK_SECTOR_SIZE / 8U; i++)
        destination[i] = source[i];
    return 0;
}

static void boot_fail(const char *message) {
    uart_puts("[BOOT] FAIL: ");
    uart_puts(message);
    uart_puts("\n");
    while (1)
        ;
}

void boot_main(void) {
    static const uint8_t expected_magic[8] = {
        'M', 'I', 'N', 'I', 'K', 'R', 'N', 'L'
    };

    uart_puts("\n[BOOT] MiniOS loader\n");
    uart_puts("[BOOT] reading kernel header\n");

    if (block_read(KERNEL_SLOT_START, sector_buffer) < 0)
        boot_fail("cannot read kernel header");

    const struct boot_image_header *header =
        (const struct boot_image_header *)sector_buffer;
    if (!bytes_equal(header->magic, expected_magic, sizeof(expected_magic)))
        boot_fail("bad kernel magic");
    if (header->version != BOOT_IMAGE_VERSION ||
        header->header_size != BOOT_IMAGE_HEADER_SIZE)
        boot_fail("unsupported kernel image");
    if (header->load_address != KERNEL_LOAD_ADDRESS)
        boot_fail("unexpected load address");
    if (header->image_size == 0 ||
        header->image_size > KERNEL_SLOT_PAYLOAD_MAX)
        boot_fail("invalid kernel size");
    if (header->entry < header->load_address ||
        header->entry >= header->load_address + header->image_size)
        boot_fail("invalid kernel entry");

    uint32_t image_size = header->image_size;
    uint32_t expected_checksum = header->checksum;
    uint64_t entry = header->entry;
    uint8_t *destination = (uint8_t *)(uintptr_t)header->load_address;
    uint32_t checksum = 0;
    uint32_t copied = 0;
    uint32_t sector = KERNEL_SLOT_START + 1U;

    uart_puts("[BOOT] loading kernel -> ");
    uart_put_hex(header->load_address);
    uart_puts("\n");

    while (copied < image_size) {
        if (block_read(sector++, sector_buffer) < 0)
            boot_fail("kernel read failed");
        uint32_t chunk = image_size - copied;
        if (chunk > BLOCK_SECTOR_SIZE)
            chunk = BLOCK_SECTOR_SIZE;
        for (uint32_t i = 0; i < chunk; i++) {
            uint8_t byte = sector_buffer[i];
            destination[copied + i] = byte;
            checksum += byte;
        }
        copied += chunk;
    }

    if (checksum != expected_checksum)
        boot_fail("checksum mismatch");

    uart_puts("[BOOT] checksum OK\n");
    uart_puts("[BOOT] jumping to kernel ");
    uart_put_hex(entry);
    uart_puts("\n");

    void (*kernel_entry)(void) = (void (*)(void))(uintptr_t)entry;
    kernel_entry();
    boot_fail("kernel returned");
}
