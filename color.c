#include "color.h"
#include <stdint.h>
#include "shell.h" // For print_string

// NOTE: The original functionality of this command is incompatible with the
// new graphics mode architecture and has been temporarily disabled to
// allow the OS to compile.

/**
 * @brief Dummy handler for the 'color' command.
 * 
 * This function is a placeholder. It informs the user that the command
 * is disabled and does nothing else, resolving the linker error.
 * 
 * @param args The arguments passed to the command (ignored).
 */
void handle_color_command(const char *args) {
    // Suppress the "unused parameter" warning from the compiler.
    (void)args;

    print_string("The 'color' command is temporarily disabled.\n");
    print_string("Its functionality is not compatible with graphics mode.\n");
}