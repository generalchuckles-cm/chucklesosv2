#ifndef GRAPHICS_H
#define GRAPHICS_H

// --- Public VGA Mode-Switching Functions ---
// Called by the shell to change modes.
void set_graphics_mode(void);
void set_text_mode(void);

// --- Public Graphical Terminal Functions ---
// Called by the kernel's stdio functions when IsGraphics == 1.

// Initializes the graphical terminal (clears screen, resets cursor).
void g_init(void);

// Clears the entire graphics screen.
void g_clear_screen(void);

// Handles a newline in graphics mode.
void g_new_line(void);

// Handles a backspace in graphics mode.
void g_backspace(void);

// Renders a single character at the current graphical cursor position.
void g_print_char(char c);

#endif // GRAPHICS_H