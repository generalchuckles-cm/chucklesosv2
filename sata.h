#ifndef SATA_H
#define SATA_H

#include <stdint.h>

// Global flag set by the driver if a usable SATA device is found
extern int sata_drive_present;

// Initializes the SATA subsystem (which in turn initializes AHCI).
void sata_init();

// Reads `count` sectors from `lba` into `buf`. Returns 0 on success.
int sata_read(uint32_t drive, uint64_t lba, uint32_t count, void* buf);

// Writes `count` sectors from `buf` to `lba`. Returns 0 on success.
int sata_write(uint32_t drive, uint64_t lba, uint32_t count, const void* buf);

#endif // SATA_H