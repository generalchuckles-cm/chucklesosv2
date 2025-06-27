#include "block.h"
#include "ata.h"
#include "sata.h"
#include "shell.h" // For print_string

// Enum to track which driver is active
typedef enum {
    ACTIVE_DRIVER_NONE,
    ACTIVE_DRIVER_PATA,
    ACTIVE_DRIVER_SATA
} ActiveDriver;

static ActiveDriver active_driver = ACTIVE_DRIVER_NONE;
int block_device_available = 0;

void block_init() {
    print_string("Probing for block devices...\n");
    
    // Try PATA first
    ata_init();
    if (ata_drive_present) {
        print_string("Block layer: Using PATA driver.\n");
        active_driver = ACTIVE_DRIVER_PATA;
        block_device_available = 1;
        return;
    }

    // If PATA fails, try SATA/AHCI
    sata_init();
    if (sata_drive_present) {
        print_string("Block layer: Using SATA/AHCI driver.\n");
        active_driver = ACTIVE_DRIVER_SATA;
        block_device_available = 1;
        return;
    }

    print_string("Block layer: No usable PATA or SATA device found.\n");
    block_device_available = 0;
}

int block_read(uint64_t lba, uint16_t count, void* buf) {
    switch (active_driver) {
        case ACTIVE_DRIVER_PATA:
            return ata_read_sectors(lba, count, buf);
        case ACTIVE_DRIVER_SATA:
            return sata_read(0, lba, count, buf);
        default:
            return -1; // No driver available
    }
}

int block_write(uint64_t lba, uint16_t count, const void* buf) {
    switch (active_driver) {
        case ACTIVE_DRIVER_PATA:
            return ata_write_sectors(lba, count, buf);
        case ACTIVE_DRIVER_SATA:
            return sata_write(0, lba, count, buf);
        default:
            return -1; // No driver available
    }
}