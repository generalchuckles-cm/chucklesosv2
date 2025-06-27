#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stddef.h>

void print_prompt(void);
void process_command(void);

// Add the extern declaration for the kernel print functions here
extern void print_string(const char *str);
extern void print_char(char c);
extern void print_hex(uint32_t val);
extern void print_int(int n);
extern void kernel_delay(uint32_t cycles); // ADDED DECLARATION

extern char input_buffer[];
extern int input_pos;

#endif