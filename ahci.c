#include "ahci.h"
#include "pci.h"
#include "shell.h"
#include "ports.h" // for ata_io_wait
#include <stddef.h>

// Required externs from kernel.c
extern char get_single_keypress();
extern void new_line();
extern void print_hex(uint32_t val);
extern void print_int(int n);
extern void* memset(void* s, int c, size_t n);
extern void print_string(const char* str);

#define AHCI_MEMORY_SIZE 0x10000 // 64KB for our AHCI structures
__attribute__((aligned(1024))) static char ahci_memory_block[AHCI_MEMORY_SIZE];

static HBA_MEM* ahci_base_memory = 0;
HBA_PORT* active_port = 0;
int ahci_drive_present = 0;

// --- PCI Definitions ---
#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROGIF_AHCI        0x01
#define PCI_VENDOR_ID          0x00
#define PCI_BAR5               0x24

// Stop command engine of a port
void stop_cmd(HBA_PORT *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    while(port->cmd & HBA_PxCMD_CR || port->cmd & HBA_PxCMD_FRE);
}

// Start command engine of a port
void start_cmd(HBA_PORT *port) {
    while(port->cmd & HBA_PxCMD_CR);
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

// Find a free command slot
int find_cmdslot(HBA_PORT *port) {
    // We only support one command at a time for simplicity
    uint32_t slots = (port->sact | port->ci);
    if ((slots & 1) == 0) return 0;
    return -1;
}

static void probe_port(HBA_MEM *abar) {
    uint32_t pi = abar->pi;
    int i = 0;
    while (i < 32) {
        if (pi & 1) {
            HBA_PORT *port = &abar->ports[i];
            uint8_t ssts = port->ssts & 0x0F;
            uint8_t ipm = (port->ssts >> 8) & 0x0F;

            if (ssts == 3 && ipm == 1) { // Check for active device
                print_string("SATA device found on port "); print_int(i); new_line();
                active_port = port;
                ahci_drive_present = 1;
                return;
            }
        }
        pi >>= 1;
        i++;
    }
}

void ahci_init() {
    uint32_t pci_bar5 = 0;
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint32_t vendor = pci_read_dword(bus, device, 0, PCI_VENDOR_ID);
            if ((vendor & 0xFFFF) == 0xFFFF) continue;
            
            uint32_t class_info = pci_read_dword(bus, device, 0, 0x08);
            if (((class_info >> 24) & 0xFF) == PCI_CLASS_MASS_STORAGE &&
                ((class_info >> 16) & 0xFF) == PCI_SUBCLASS_SATA &&
                ((class_info >> 8) & 0xFF) == PCI_PROGIF_AHCI) {
                pci_bar5 = pci_read_dword(bus, device, 0, PCI_BAR5);
                goto found;
            }
        }
    }
found:
    if (!pci_bar5) return;
    
    ahci_base_memory = (HBA_MEM*)(pci_bar5 & 0xFFFFFFF0);
    
    probe_port(ahci_base_memory);
    if (!active_port) return;

    stop_cmd(active_port);

    uint32_t mem_base = (uint32_t)ahci_memory_block;
    
    active_port->clb = mem_base;
    active_port->clbu = 0;
    memset((void*)active_port->clb, 0, 1024);

    active_port->fb = mem_base + 1024;
    active_port->fbu = 0;
    memset((void*)active_port->fb, 0, 256);

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)active_port->clb;
    cmdheader->ctba = mem_base + 4096;
    cmdheader->ctbau = 0;
    memset((void*)cmdheader->ctba, 0, 256);

    start_cmd(active_port);
}

int ahci_read(HBA_PORT *port, uint64_t lba, uint32_t count, void *buf) {
    port->is = (uint32_t)-1;
    int slot = find_cmdslot(port);
    if (slot == -1) return -1;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmdheader->w = 0; // Read
    cmdheader->prdtl = 1;

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(cmdheader->ctba);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    cmdtbl->prdt_entry[0].dba = (uint32_t)buf;
    cmdtbl->prdt_entry[0].dbau = 0;
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;

    FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
    cmdfis->fis_type = 0x27;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_READ_DMA_EXT;

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);

    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;

    port->ci = 1 << slot;

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return -1;
    }

    if (port->is & HBA_PxIS_TFES) return -1;
    return 0;
}

int ahci_write(HBA_PORT *port, uint64_t lba, uint32_t count, const void *buf) {
    port->is = (uint32_t)-1;
    int slot = find_cmdslot(port);
    if (slot == -1) return -1;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmdheader->w = 1; // Write
    cmdheader->prdtl = 1;

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(cmdheader->ctba);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    cmdtbl->prdt_entry[0].dba = (uint32_t)buf;
    cmdtbl->prdt_entry[0].dbau = 0;
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;

    FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
    cmdfis->fis_type = 0x27;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_WRITE_DMA_EXT;

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);

    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;

    port->ci = 1 << slot;

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return -1;
    }

    if (port->is & HBA_PxIS_TFES) return -1;
    return 0;
}