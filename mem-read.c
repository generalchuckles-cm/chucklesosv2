#include <stdint.h>
#include <stddef.h>
#include "shell.h"
#include <stdbool.h>

// Print hex helper - now lives in kernel.c and is declared in shell.h
extern void print_string(const char *str);
extern void print_char(char c);
extern void print_int(int n);
extern void new_line(void);
extern void print_hex(uint32_t val); // Declaration from shell.h

// The duplicate print_hex function has been REMOVED from here.

// Parse hex string, supports optional "0x" prefix
bool parse_hex(const char *str, uint32_t *out) {
    uint32_t result = 0;
    const char *p = str;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    if (*p == '\0') return false;
    while (*p) {
        char c = *p++;
        uint8_t val = 0xFF;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else return false;
        if (val == 0xFF) return false;
        result = (result << 4) | val;
    }
    *out = result;
    return true;
}

// Extern input buffer and position
extern char input_buffer[];
extern int input_pos;

// Run the mr command
void mem_read_command(const char *args) {
    // args points to string after "mr "
    while (*args == ' ') args++;

    if (*args == '\0') {
        print_string("Usage: mr <hex address>\n");
        return;
    }

    uint32_t addr = 0;
    if (!parse_hex(args, &addr)) {
        print_string("Invalid hex address\n");
        return;
    }

    // Unsafe direct memory read (mapped to VGA memory? be careful)
    volatile uint32_t *ptr = (volatile uint32_t *)addr;
    uint32_t val = *ptr;

    print_string("Value at ");
    print_string(args);
    print_string(": ");
    print_hex(val);
    new_line();
}