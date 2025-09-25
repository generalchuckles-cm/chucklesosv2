#include "graphics.h"
#include "charset.h"
#include "ports.h"
#include "shell.h"
#include "extrainclude.h"
#include <stdint.h>

#define GFX_VIDEO_MEMORY    0xA0000
#define GFX_SCREEN_WIDTH    320
#define GFX_SCREEN_HEIGHT   200
#define FONT_WIDTH          8
#define FONT_HEIGHT         8
#define GFX_COLS            (GFX_SCREEN_WIDTH / FONT_WIDTH)   // 40
#define GFX_ROWS            (GFX_SCREEN_HEIGHT / FONT_HEIGHT) // 25

// --- Global state for our graphics terminal ---
static int g_cursor_x = 0;
static int g_cursor_y = 0;
static uint8_t g_fg_color = 15; // White
static uint8_t g_bg_color = 1;  // Blue

// --- Low-level Drawing Function ---

// Draws a single character from the charset at a specific pixel coordinate.
static void g_put_char_at_xy(char c, int x_px, int y_px, uint8_t fg, uint8_t bg) {
    if (c < 0) return; // Ignore non-ASCII
    volatile uint8_t* vram = (uint8_t*)GFX_VIDEO_MEMORY;
    const uint8_t* font_char = default_font[(uint8_t)c];

    for (int y = 0; y < FONT_HEIGHT; y++) {
        uint8_t row = font_char[y];
        for (int x = 0; x < FONT_WIDTH; x++) {
            uint32_t offset = (y_px + y) * GFX_SCREEN_WIDTH + (x_px + x);
            if (offset < GFX_SCREEN_WIDTH * GFX_SCREEN_HEIGHT) {
                if ((row >> (7 - x)) & 1) {
                    vram[offset] = fg;
                } else {
                    vram[offset] = bg;
                }
            }
        }
    }
}

// --- Public Graphical Terminal Functions (called by kernel.c) ---

// Scrolls the entire screen up by one character row.
static void g_scroll() {
    void* dest = (void*)GFX_VIDEO_MEMORY;
    const void* src = (void*)(GFX_VIDEO_MEMORY + GFX_SCREEN_WIDTH * FONT_HEIGHT);
    size_t num_bytes = GFX_SCREEN_WIDTH * (GFX_SCREEN_HEIGHT - FONT_HEIGHT);
    memmove(dest, src, num_bytes);

    // Clear the last line
    void* last_line = (void*)(GFX_VIDEO_MEMORY + GFX_SCREEN_WIDTH * (GFX_ROWS - 1) * FONT_HEIGHT);
    memset(last_line, g_bg_color, GFX_SCREEN_WIDTH * FONT_HEIGHT);
}

void g_clear_screen() {
    memset((void*)GFX_VIDEO_MEMORY, g_bg_color, GFX_SCREEN_WIDTH * GFX_SCREEN_HEIGHT);
    g_cursor_x = 0;
    g_cursor_y = 0;
}

void g_init() {
    g_clear_screen();
}

void g_new_line() {
    g_cursor_x = 0;
    g_cursor_y++;
    if (g_cursor_y >= GFX_ROWS) {
        g_scroll();
        g_cursor_y = GFX_ROWS - 1;
    }
}

void g_backspace() {
    if (g_cursor_x > 0) {
        g_cursor_x--;
        g_put_char_at_xy(' ', g_cursor_x * FONT_WIDTH, g_cursor_y * FONT_HEIGHT, g_fg_color, g_bg_color);
    }
    // Could add logic to wrap to previous line if desired, but this is simpler.
}

void g_print_char(char c) {
    if (c == '\n') {
        g_new_line();
    } else if (c == '\b') {
        g_backspace();
    } else {
        g_put_char_at_xy(c, g_cursor_x * FONT_WIDTH, g_cursor_y * FONT_HEIGHT, g_fg_color, g_bg_color);
        g_cursor_x++;
        if (g_cursor_x >= GFX_COLS) {
            g_new_line();
        }
    }
}

// --- Mode Switching Implementations (called by shell.c) ---

void set_graphics_mode() {
    outb(0x3C2, 0x63);
    outb(0x3C4, 0); outb(0x3C5, 0x03);
    outb(0x3C4, 1); outb(0x3C5, 0x01);
    outb(0x3C4, 2); outb(0x3C5, 0x0F);
    outb(0x3C4, 3); outb(0x3C5, 0x00);
    outb(0x3C4, 4); outb(0x3C5, 0x0E);
    outb(0x3D4, 0x03); outb(0x3D5, inb(0x3D5) | 0x80);
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & ~0x80);
    uint8_t crtc[] = {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9C, 0x0E, 0x8F, 0x28,	0x40, 0x96, 0xB9, 0xA3, 0xFF};
    for(uint8_t i = 0; i < sizeof(crtc); i++) { outb(0x3D4, i); outb(0x3D5, crtc[i]); }
    outb(0x3CE, 0); outb(0x3CF, 0x00);
    outb(0x3CE, 1); outb(0x3CF, 0x00);
    outb(0x3CE, 2); outb(0x3CF, 0x00);
    outb(0x3CE, 3); outb(0x3CF, 0x00);
    outb(0x3CE, 4); outb(0x3CF, 0x00);
    outb(0x3CE, 5); outb(0x3CF, 0x40);
    outb(0x3CE, 6); outb(0x3CF, 0x05);
    outb(0x3CE, 7); outb(0x3CF, 0x0F);
    outb(0x3CE, 8); outb(0x3CF, 0xFF);
    inb(0x3DA);
    for(uint8_t i=0; i<0x10; i++) { outb(0x3C0, i); outb(0x3C0, i); }
    outb(0x3C0, 0x10); outb(0x3C0, 0x41);
    outb(0x3C0, 0x11); outb(0x3C0, 0xFF);
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F);
    outb(0x3C0, 0x13); outb(0x3C0, 0x00);
    outb(0x3C0, 0x14); outb(0x3C0, 0x00);
    outb(0x3C0, 0x20);
}

void set_text_mode() {
    outb(0x3C2, 0x67);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x03);
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & 0x7F);
    uint8_t crtc[] = {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3, 0xFF};
    for (uint8_t i = 0; i < sizeof(crtc); i++) { outb(0x3D4, i); outb(0x3D5, crtc[i]); }
    outb(0x3CE, 0x00); outb(0x3CF, 0x00);
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);
    outb(0x3CE, 0x02); outb(0x3CF, 0x00);
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);
    outb(0x3CE, 0x07); outb(0x3CF, 0x00);
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);
    inb(0x3DA);
    outb(0x3C0, 0x10); outb(0x3C0, 0x01);
    for (uint8_t i = 0; i < 0x10; i++) { outb(0x3C0, i); outb(0x3C0, i); }
    outb(0x3C0, 0x11); outb(0x3C0, 0x00);
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F);
    outb(0x3C0, 0x13); outb(0x3C0, 0x00);
    outb(0x3C0, 0x14); outb(0x3C0, 0x00);
    outb(0x3C0, 0x20);
}