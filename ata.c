
#include "ata.h"
#include "ports.h"

// You must declare your print function as extern so this file can use it.
extern void print_string(const char* str);
extern void print_char(char c);
extern void new_line();
// This new function is needed from kernel.c for the y/n prompt
extern char get_single_keypress();

// Global state variable, 1 if drive is present, 0 otherwise.
int ata_drive_present = 0;

// Timeout for ATA commands, a simple busy-wait counter.
#define ATA_TIMEOUT 10000000

static void ata_io_wait() { // Wait 400ns by reading the status port 4 times
    inb(ATA_PORT_STATUS);
    inb(ATA_PORT_STATUS);
    inb(ATA_PORT_STATUS);
    inb(ATA_PORT_STATUS);
}

// Polls the status port until the busy bit is cleared.
// Returns 0 on success, or an error code on failure/timeout.
static int ata_wait_not_busy() {
    for(int i = 0; i < ATA_TIMEOUT; i++) {
        if (!(inb(ATA_PORT_STATUS) & ATA_STATUS_BUSY)) {
            return 0; // Success, not busy
        }
    }
    print_string("ATA: BSY timeout!\n");
    return ATA_STATUS_TIMEOUT;
}

// Polls until the drive is ready for data transfer (DRQ is set).
// Returns 0 on success, or an error code on failure/timeout.
static int ata_wait_drq() {
    for(int i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t status = inb(ATA_PORT_STATUS);
        if (status & ATA_STATUS_ERR) {
            print_string("ATA: ERR set!\n");
            return ATA_STATUS_ERR;
        }
        if (status & ATA_STATUS_DRQ) {
            return 0; // Success, DRQ set
        }
    }
    print_string("ATA: DRQ timeout!\n");
    return ATA_STATUS_TIMEOUT;
}

// This function is completely rewritten
void ata_init() {
    print_string("Scanning for ATA devices...\n");
    ata_drive_present = 0;

    // --- Select Master Drive ---
    outb(ATA_PORT_DRIVE_HEAD, 0xA0);
    ata_io_wait();

    // Check if any device is present on the bus
    if (inb(ATA_PORT_STATUS) == 0xFF) {
        print_string("No device on Primary Master.\n");
        return;
    }

    // --- Send IDENTIFY Command ---
    outb(ATA_PORT_SECTOR_COUNT, 0);
    outb(ATA_PORT_LBA_LOW, 0);
    outb(ATA_PORT_LBA_MID, 0);
    outb(ATA_PORT_LBA_HIGH, 0);
    outb(ATA_PORT_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait();
    
    // Check status again
    if (inb(ATA_PORT_STATUS) == 0x00) {
        print_string("No device responded to IDENTIFY.\n");
        return;
    }

    if (ata_wait_not_busy() != 0) {
        print_string("Device hung after IDENTIFY command.\n");
        return;
    }

    if (!(inb(ATA_PORT_STATUS) & ATA_STATUS_DRQ)) {
        print_string("Device did not set DRQ after IDENTIFY. Likely not ATA.\n");
        return;
    }

    // --- Read IDENTIFY data ---
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(ATA_PORT_DATA);
    }
    
    // --- Extract and print model name ---
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2] = identify_data[27 + i] >> 8;
        model[i * 2 + 1] = identify_data[27 + i] & 0xFF;
    }
    model[40] = '\0';
    
    // Trim trailing spaces from model name for cleaner printing
    for (int i = 39; i >= 0; i--) {
        if (model[i] != ' ') break;
        model[i] = '\0';
    }

    print_string("Device Detected: ");
    print_string(model);
    print_string("\nScan this drive? (y/n): ");

    char response = get_single_keypress();
    print_char(response);
    new_line();
    
    if (response == 'y' || response == 'Y') {
        // Check if it's an ATA device (not ATAPI) from the IDENTIFY data
        // Bit 15 of word 0 is 0 for ATA, 1 for ATAPI
        if (identify_data[0] & 0x8000) {
            print_string("This is an ATAPI device (like a CD-ROM) and is not supported for file storage.\n");
        } else {
            print_string("ATA Hard Disk selected. Filesystem will be initialized.\n");
            ata_drive_present = 1;
        }
    } else {
        print_string("Skipping device.\n");
    }
}


int ata_read_sectors(uint32_t lba, uint8_t num_sectors, void* buffer) {
    if (!ata_drive_present) return -1;
    if (ata_wait_not_busy() != 0) return -1;

    // Select master drive (0xE0 for LBA mode) and send high 4 bits of LBA
    outb(ATA_PORT_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_wait();
    outb(ATA_PORT_SECTOR_COUNT, num_sectors);
    outb(ATA_PORT_LBA_LOW, (uint8_t)lba);
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_PORT_COMMAND, ATA_CMD_READ_SECTORS);

    uint16_t* target = (uint16_t*)buffer;
    int ret;
    for (int s = 0; s < num_sectors; s++) {
        if ((ret = ata_wait_not_busy()) != 0) return ret;
        if ((ret = ata_wait_drq()) != 0) return ret;

        for (int i = 0; i < 256; i++) {
            target[i] = inw(ATA_PORT_DATA);
        }
        target += 256;
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t num_sectors, const void* buffer) {
    if (!ata_drive_present) return -1;
    if (ata_wait_not_busy() != 0) return -1;

    outb(ATA_PORT_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_wait();
    outb(ATA_PORT_SECTOR_COUNT, num_sectors);
    outb(ATA_PORT_LBA_LOW, (uint8_t)lba);
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_PORT_COMMAND, ATA_CMD_WRITE_SECTORS);

    const uint16_t* source = (const uint16_t*)buffer;
    int ret;
    for (int s = 0; s < num_sectors; s++) {
        if ((ret = ata_wait_not_busy()) != 0) return ret;
        if ((ret = ata_wait_drq()) != 0) return ret;

        for (int i = 0; i < 256; i++) {
            outw(ATA_PORT_DATA, source[i]);
        }
        source += 256;
    }

    // After writing, the drive may need to cache. We can flush it.
    // For simplicity, we just wait for it to not be busy again.
    if ((ret = ata_wait_not_busy()) != 0) return ret;

    return 0;
}