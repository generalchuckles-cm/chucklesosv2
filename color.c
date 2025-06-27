
#include "color.h"
#include <stdint.h>
#include "shell.h" // For print_string, new_line, etc.

// These are global variables defined in kernel.c
// We declare them as extern to be able to modify them.
extern uint8_t terminal_fg_color;
extern uint8_t terminal_bg_color;

// We also need the new function from kernel.c to apply the changes.
extern void update_screen_colors(void);

static void print_usage() {
    print_string("Usage: color <target> <value>\n");
    print_string("  target: f (foreground) or b (background)\n");
    print_string("  value:  0-15\n");
    print_string("    0: Black, 1: Blue, 2: Green, 3: Cyan, 4: Red\n");
    print_string("    5: Magenta, 6: Brown, 7: Light Grey, 8: Dark Grey\n");
    print_string("    9: Light Blue, 10: Light Green, 11: Light Cyan\n");
    print_string("    12: Light Red, 13: Light Magenta, 14: Yellow, 15: White\n");
}

void handle_color_command(const char *args) {
    const char* p = args;

    // Skip leading whitespace
    while (*p == ' ') {
        p++;
    }

    if (*p == '\0') {
        print_usage();
        return;
    }

    char target = *p++;
    if (target != 'f' && target != 'b') {
        print_usage();
        return;
    }

    // Skip whitespace between target and value
    while (*p == ' ') {
        p++;
    }

    if (*p < '0' || *p > '9') {
        print_usage();
        return;
    }

    // Simple atoi to parse the color value
    int value = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }

    if (value < 0 || value > 15) {
        print_string("Error: Color value must be between 0 and 15.\n");
        return;
    }

    if (target == 'f') {
        terminal_fg_color = (uint8_t)value;
    } else { // target == 'b'
        terminal_bg_color = (uint8_t)value;
    }

    // After changing the color variables, apply the changes to the whole screen.
    update_screen_colors();
}