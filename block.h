#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

// Global flag to indicate if any usable block device was found.
extern int block_device_available;

// Initializes the block device subsystem.
// It will probe for ATA and SATA devices and select one to use.
void block_init();

// Reads `count` sectors from `lba` into `buf`. Returns 0 on success.
int block_read(uint64_t lba, uint16_t count, void* buf);

// Writes `count` sectors from `buf` to `lba`. Returns 0 on success.
int block_write(uint64_t lba, uint16_t count, const void* buf);

#endif // BLOCK_H