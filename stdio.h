#ifndef STDIO_H
#define STDIO_H

#include <stdint.h>

// --- Kernel-level I/O Functions ---
// These functions directly interact with the VGA buffer or I/O ports.

void print_string(const char *str);
void print_char(char c);
void print_int(int n);
void print_hex(uint32_t val);
void new_line(void);
void clear_screen(void);
void get_user_input(char* buffer, int max_len);
char get_single_keypress(void);

#endif // STDIO_H