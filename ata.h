#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Primary ATA Bus I/O Ports
#define ATA_PORT_DATA          0x1F0
#define ATA_PORT_ERROR         0x1F1
#define ATA_PORT_SECTOR_COUNT  0x1F2
#define ATA_PORT_LBA_LOW       0x1F3
#define ATA_PORT_LBA_MID       0x1F4
#define ATA_PORT_LBA_HIGH      0x1F5
#define ATA_PORT_DRIVE_HEAD    0x1F6
#define ATA_PORT_STATUS        0x1F7
#define ATA_PORT_COMMAND       0x1F7

// Status Bits
#define ATA_STATUS_BUSY 0x80
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_TIMEOUT 0x04 // Custom status for our driver
#define ATA_STATUS_ERR  0x01

// Commands
#define ATA_CMD_READ_SECTORS   0x20
#define ATA_CMD_WRITE_SECTORS  0x30
#define ATA_CMD_IDENTIFY       0xEC

// Public Functions

// Global state: 1 if drive is present and responsive, 0 otherwise.
extern int ata_drive_present;

void ata_init();
int ata_read_sectors(uint32_t lba, uint8_t num_sectors, void* buffer);
int ata_write_sectors(uint32_t lba, uint8_t num_sectors, const void* buffer);

#endif // ATA_H