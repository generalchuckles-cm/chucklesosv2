#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>

// --- AHCI Structure Definitions ---
#define ATA_CMD_READ_DMA_EXT  0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define HBA_PxCMD_ST  0x0001
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_CR  0x8000
#define HBA_PxIS_TFES (1 << 30) // Task File Error Status

typedef volatile struct {
    uint32_t clb;       // Command List Base Address, 1K-byte aligned
    uint32_t clbu;      // Command List Base Address Upper 32 bits
    uint32_t fb;        // FIS Base Address, 256-byte aligned
    uint32_t fbu;       // FIS Base Address Upper 32 bits
    uint32_t is;        // Interrupt Status
    uint32_t ie;        // Interrupt Enable
    uint32_t cmd;       // Command and Status
    uint32_t rsv0;      // Reserved
    uint32_t tfd;       // Task File Data
    uint32_t sig;       // Signature
    uint32_t ssts;      // SATA Status (SCR0:SStatus)
    uint32_t sctl;      // SATA Control (SCR2:SControl)
    uint32_t serr;      // SATA Error (SCR1:SError)
    uint32_t sact;      // SATA Active (SCR3:SActive)
    uint32_t ci;        // Command Issue
    uint32_t sntf;      // SATA Notification (SCR4:SNotification)
    uint32_t fbs;       // FIS-based Switching Control
    uint32_t rsv1[11];  // Reserved
    uint32_t vendor[4]; // Vendor-specific
} HBA_PORT;

typedef volatile struct {
    uint32_t cap;       // Host Capability
    uint32_t ghc;       // Global Host Control
    uint32_t is;        // Interrupt Status
    uint32_t pi;        // Port Implemented
    uint32_t vs;        // Version
    uint32_t ccc_ctl;   // Command Completion Coalescing Control
    uint32_t ccc_pts;   // Command Completion Coalescing Ports
    uint32_t em_loc;    // Enclosure Management Location
    uint32_t em_ctl;    // Enclosure Management Control
    uint32_t cap2;      // Host Capabilities Extended
    uint32_t bohc;      // BIOS/OS Handoff Control and Status
    uint8_t  rsv[0x74]; // Reserved
    uint8_t  vendor[0x60]; // Vendor-specific registers
    HBA_PORT ports[32]; // 32 ports
} HBA_MEM;

typedef struct {
    // 0x00
    uint8_t fis_type;   // FIS_TYPE_REG_H2D
    uint8_t pmport:4;   // Port multiplier
    uint8_t rsv0:3;     // Reserved
    uint8_t c:1;        // 1: Command, 0: Control
    uint8_t command;    // Command register
    uint8_t featurel;   // Feature register, 7:0
    // 0x04
    uint8_t lba0;       // LBA low register, 7:0
    uint8_t lba1;       // LBA mid register, 15:8
    uint8_t lba2;       // LBA high register, 23:16
    uint8_t device;     // Device register
    // 0x08
    uint8_t lba3;       // LBA register, 31:24
    uint8_t lba4;       // LBA register, 39:32
    uint8_t lba5;       // LBA register, 47:40
    uint8_t featureh;   // Feature register, 15:8
    // 0x0C
    uint8_t countl;     // Count register, 7:0
    uint8_t counth;     // Count register, 15:8
    uint8_t icc;        // Isochronous command completion
    uint8_t control;    // Control register
    // 0x10
    uint8_t rsv1[4];    // Reserved
} FIS_REG_H2D;

typedef struct {
    uint32_t dba;      // Data base address (lower 32 bits)
    uint32_t dbau;     // Data base address (upper 32 bits)
    uint32_t rsv0;     // Reserved
    uint32_t dbc;      // Byte count (0-based, max 4MB-1)
} HBA_PRDT_ENTRY;

// ** THIS STRUCT IS NOW FULLY CORRECTED **
typedef struct {
    uint8_t  cfis[64];    // Command FIS
    uint8_t  acmd[16];    // ATAPI Command
    uint8_t  rsv[48];     // Reserved
    HBA_PRDT_ENTRY prdt_entry[1]; // Physical region descriptor table
} HBA_CMD_TBL;

// ** THIS STRUCT IS NOW FULLY CORRECTED **
typedef struct {
    uint16_t cfl:5;       // Command FIS Length in DWORDS, 2-16
    uint16_t a:1;         // ATAPI
    uint16_t w:1;         // Write, 1: H2D, 0: D2H
    uint16_t p:1;         // Prefetchable
    uint16_t r:1;         // Reset
    uint16_t b:1;         // BIST
    uint16_t c:1;         // Clear Busy upon R_OK
    uint16_t rsv0:1;      // Reserved
    uint16_t pmp:4;       // Port Multiplier Port
    uint16_t prdtl;       // Physical Region Descriptor Table Length
    uint32_t prdbc;       // Physical Region Descriptor Byte Count
    uint32_t ctba;        // Command Table Base Address
    uint32_t ctbau;       // Command Table Upper 32 bits
    uint32_t rsv1[4];     // Reserved
} HBA_CMD_HEADER;

// --- Public Function Prototypes ---
extern int ahci_drive_present; // Flag if a usable port was found

void ahci_init();
int ahci_read(HBA_PORT *port, uint64_t lba, uint32_t count, void *buf);
int ahci_write(HBA_PORT *port, uint64_t lba, uint32_t count, const void *buf);

#endif // AHCI_H