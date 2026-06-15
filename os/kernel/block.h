#ifndef MINIOS_BLOCK_H
#define MINIOS_BLOCK_H

#include <stdint.h>

#define BLOCK_SECTOR_SIZE 512
#define BLOCK_SECTOR_COUNT 2048

int block_read(uint32_t sector, void *buffer);
int block_write(uint32_t sector, const void *buffer);

#endif
