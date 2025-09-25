#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stddef.h>
#include "stdio.h"
#include "extrainclude.h"

void print_prompt(void);
void process_command(void);

// Add the extern declaration for the kernel delay function
extern void kernel_delay(uint32_t cycles);

extern char input_buffer[];
extern int input_pos;

#endif