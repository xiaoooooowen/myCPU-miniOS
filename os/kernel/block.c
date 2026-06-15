#include "block.h"

#define BLOCK_BASE       0x10001000UL
#define BLOCK_REG_SECTOR (BLOCK_BASE + 0x00)
#define BLOCK_REG_CMD    (BLOCK_BASE + 0x08)
#define BLOCK_REG_STATUS (BLOCK_BASE + 0x0c)
#define BLOCK_DATA       (BLOCK_BASE + 0x100)

#define BLOCK_CMD_READ  1U
#define BLOCK_CMD_WRITE 2U

int block_read(uint32_t sector, void *buffer) {
    if (buffer == 0 || sector >= BLOCK_SECTOR_COUNT)
        return -1;

    *(volatile uint64_t *)BLOCK_REG_SECTOR = sector;
    *(volatile uint32_t *)BLOCK_REG_CMD = BLOCK_CMD_READ;
    if (*(volatile uint32_t *)BLOCK_REG_STATUS != 0)
        return -1;

    uint64_t *dst = (uint64_t *)buffer;
    volatile uint64_t *src = (volatile uint64_t *)BLOCK_DATA;
    for (int i = 0; i < BLOCK_SECTOR_SIZE / 8; i++)
        dst[i] = src[i];
    return 0;
}

int block_write(uint32_t sector, const void *buffer) {
    if (buffer == 0 || sector >= BLOCK_SECTOR_COUNT)
        return -1;

    const uint64_t *src = (const uint64_t *)buffer;
    volatile uint64_t *dst = (volatile uint64_t *)BLOCK_DATA;
    for (int i = 0; i < BLOCK_SECTOR_SIZE / 8; i++)
        dst[i] = src[i];

    *(volatile uint64_t *)BLOCK_REG_SECTOR = sector;
    *(volatile uint32_t *)BLOCK_REG_CMD = BLOCK_CMD_WRITE;
    return *(volatile uint32_t *)BLOCK_REG_STATUS == 0 ? 0 : -1;
}
