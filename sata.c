#include "sata.h"
#include "ahci.h"
#include "shell.h"

int sata_drive_present = 0; // ADDED: The missing definition of the variable
extern HBA_PORT* active_port; // From ahci.c

void sata_init() {
    ahci_init();
    if (ahci_drive_present) {
        sata_drive_present = 1;
    }
}

int sata_read(uint32_t drive, uint64_t lba, uint32_t count, void* buf) {
    if (!sata_drive_present || !active_port) return -1;
    // 'drive' is ignored for now as we only support one
    return ahci_read(active_port, lba, count, buf);
}

int sata_write(uint32_t drive, uint64_t lba, uint32_t count, const void* buf) {
    if (!sata_drive_present || !active_port) return -1;
    // 'drive' is ignored for now as we only support one
    return ahci_write(active_port, lba, count, buf);
}