#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Reads a 32-bit double word from the configuration space of a PCI device.
uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

#endif // PCI_H