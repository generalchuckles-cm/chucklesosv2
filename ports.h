#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

// Note: These functions must be defined in one of your .c files,
// preferably kernel.c since it already has some.

// Write a byte to a port
void outb(uint16_t port, uint8_t val);
// Read a byte from a port
uint8_t inb(uint16_t port);
// Write a word (16-bit) to a port
void outw(uint16_t port, uint16_t val);
// Read a word (16-bit) from a port
uint16_t inw(uint16_t port);
// Write a dword (32-bit) to a port
void outl(uint16_t port, uint32_t val);
// Read a dword (32-bit) from a port
uint32_t inl(uint16_t port);


#endif // PORTS_H