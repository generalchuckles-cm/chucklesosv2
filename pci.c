#include "pci.h"
#include "ports.h"

// PCI Configuration Address Port
#define PCI_CONFIG_ADDRESS 0xCF8
// PCI Configuration Data Port
#define PCI_CONFIG_DATA    0xCFC

uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    // Create the address dword
    uint32_t address = (uint32_t)((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       (offset & 0xFC) | // lower 2 bits must be 0
                       0x80000000;       // Enable bit

    // Write the address to the address port
    outl(PCI_CONFIG_ADDRESS, address);

    // Read the data from the data port
    return inl(PCI_CONFIG_DATA);
}