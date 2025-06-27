#include "cdg_player.h"
#include "hdd_fs.h"
#include "shell.h"
#include "ports.h"
#include <stdint.h>
#include <stddef.h>

// Extern functions and variables from the kernel
extern void clear_screen();
extern unsigned short *terminal_buffer;
extern uint8_t terminal_fg_color;
extern uint8_t terminal_bg_color;
extern void kernel_delay(uint32_t cycles);
extern void* memset(void *s, int c, size_t n);

// CD+G Constants
#define CDG_PACKET_SIZE      24
#define CDG_COMMAND_MASK     0x3F
#define CDG_COMMAND          0x09

// CD+G Instructions for Command 0x09
#define CDG_INSTR_MEM_PRESET      1
#define CDG_INSTR_BORDER_PRESET   2
#define CDG_INSTR_TILE_BLOCK      6  // Normal tile draw
#define CDG_INSTR_TILE_BLOCK_XOR  38 // XOR tile draw
#define CDG_INSTR_SCROLL_PRESET   20
#define CDG_INSTR_SCROLL_COPY     24
#define CDG_INSTR_LOAD_CLUT_LOW   30
#define CDG_INSTR_LOAD_CLUT_HIGH  31

// Screen dimensions
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

// CD+G native resolution
#define CDG_WIDTH   300
#define CDG_HEIGHT  216
#define CDG_TILE_W   6
#define CDG_TILE_H   12
#define CDG_COLS (CDG_WIDTH / CDG_TILE_W)  // 50
#define CDG_ROWS (CDG_HEIGHT / CDG_TILE_H) // 18

#define MAX_FILE_SIZE (CDG_PACKET_SIZE * 10000)

static uint8_t file_buffer[MAX_FILE_SIZE];
static uint8_t cdg_palette[16];

// Map a 4-bit RGB color to the closest 16-color VGA/terminal palette index.
static uint8_t map_rgb_to_vga16(uint8_t r, uint8_t g, uint8_t b) {
    if (r == 0 && g == 0 && b == 0) return 0;
    if (r == g && g == b) {
        if (r <= 2) return 0;
        else if (r <= 5) return 8;
        else if (r <= 10) return 7;
        else return 15;
    }
    uint8_t intensity = (r > 8 || g > 8 || b > 8) ? 8 : 0;
    uint8_t vga_r = (r > (intensity ? 6 : 3)) ? 4 : 0;
    uint8_t vga_g = (g > (intensity ? 6 : 3)) ? 2 : 0;
    uint8_t vga_b = (b > (intensity ? 6 : 3)) ? 1 : 0;
    uint8_t color = intensity | vga_r | vga_g | vga_b;
    if (color == 0 && (r > 1 || g > 1 || b > 1)) return 8;
    return color;
}

// Map CDG palette index to terminal 16-color palette
static uint8_t map_color(uint8_t cdg_color) {
    return cdg_palette[cdg_color & 0x0F];
}

// *** CHANGED ***
// Draw a tile using both foreground and background colors directly to the terminal buffer.
static void draw_tile(uint8_t col, uint8_t row, uint8_t bg_color, uint8_t fg_color, int is_xor) {
    int sx = (col * SCREEN_WIDTH) / CDG_COLS;
    int sy = (row * SCREEN_HEIGHT) / CDG_ROWS;

    if (sx < SCREEN_WIDTH && sy < SCREEN_HEIGHT) {
        char ch = 219; // Full block character
        
        // XOR is tricky with this model. A simple implementation is to just draw.
        // A more complex one might swap fg/bg based on the existing color.
        // For now, we ignore the XOR flag for simplicity and correctness.
        uint8_t attr = (bg_color << 4) | (fg_color & 0x0F);
        terminal_buffer[sy * SCREEN_WIDTH + sx] = (unsigned short)ch | ((unsigned short)attr << 8);
    }
}


// *** CHANGED ***
// Handle a single CDG packet, drawing directly to the terminal buffer.
static void handle_packet(const uint8_t* packet) {
    uint8_t command = packet[0] & CDG_COMMAND_MASK;
    uint8_t instr   = packet[1] & CDG_COMMAND_MASK;
    const uint8_t* data = &packet[4];

    if (command != CDG_COMMAND) return;

    switch (instr) {
        case CDG_INSTR_MEM_PRESET: {
            uint8_t color = map_color(data[0] & 0x0F);
            // Create a character with the background and foreground as the same color.
            uint8_t attr = (color << 4) | (color & 0x0F);
            unsigned short val = (' ' << 8) | attr;
            // Fill the entire terminal buffer.
            for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
                terminal_buffer[i] = (val << 8) | ' ';
            }
            break;
        }
        
        case CDG_INSTR_TILE_BLOCK:
        case CDG_INSTR_TILE_BLOCK_XOR: {
            uint8_t color0 = map_color(data[0] & 0x0F);
            uint8_t color1 = map_color(data[1] & 0x0F);
            uint8_t row = data[2] & 0x1F;
            uint8_t col = data[3] & 0x3F;

            int color1_pixels = 0;
            for (int i = 0; i < 12; i++) {
                uint8_t byte = data[4 + i];
                for (int j = 0; j < 6; j++) {
                    if ((byte >> (5 - j)) & 1) {
                        color1_pixels++;
                    }
                }
            }
            
            // Use the more dominant color as the foreground and the other as the background.
            if (color1_pixels >= 36) { // color1 is dominant
                draw_tile(col, row, color0, color1, (instr == CDG_INSTR_TILE_BLOCK_XOR));
            } else { // color0 is dominant
                draw_tile(col, row, color1, color0, (instr == CDG_INSTR_TILE_BLOCK_XOR));
            }
            break;
        }

        case CDG_INSTR_LOAD_CLUT_LOW:
        case CDG_INSTR_LOAD_CLUT_HIGH: {
            int start_index = (instr == CDG_INSTR_LOAD_CLUT_LOW) ? 0 : 8;
            for (int i = 0; i < 8; i++) {
                const uint8_t* color_data = &data[i * 2];
                uint8_t r = (color_data[0] & 0x3C) >> 2;
                uint8_t g = ((color_data[0] & 0x03) << 2) | ((color_data[1] & 0x30) >> 4);
                uint8_t b = color_data[1] & 0x0F;
                cdg_palette[start_index + i] = map_rgb_to_vga16(r, g, b);
            }
            break;
        }

        // Other cases are ignored for simplicity.
        default:
            break;
    }
}

void cdg_player_start(const char* filename) {
    print_string("Loading CD+G file: ");
    print_string(filename);
    new_line();

    int bytes_read = fs_read_file(filename, (char*)file_buffer);
    if (bytes_read <= 0) {
        print_string("Error: Could not read file or file is empty.\n");
        return;
    }

    if (bytes_read % CDG_PACKET_SIZE != 0) {
        print_string("Warning: File size is not a multiple of 24 bytes.\n");
    }

    uint8_t old_fg = terminal_fg_color;
    uint8_t old_bg = terminal_bg_color;
    
    clear_screen();
    // Initialize default palette
    for(int i = 0; i < 16; i++) cdg_palette[i] = i;

    print_string("Playing... Press ESC to exit.\n");
    kernel_delay(50000000); // ~1 sec delay
    clear_screen();

    int total_packets = bytes_read / CDG_PACKET_SIZE;
    int packet_batch_size = 25; // Process this many packets before a delay

    for (int i = 0; i < total_packets; i++) {
        if ((inb(0x64) & 1) && inb(0x60) == 1) { // Check for ESC
            break;
        }

        const uint8_t* packet = &file_buffer[i * CDG_PACKET_SIZE];
        handle_packet(packet);

        // *** CHANGED ***
        // Instead of rendering, we now just delay periodically.
        // The standard rate is ~750 packets/sec. We process a batch and then delay.
        if (i % packet_batch_size == 0) {
            // Delay to approximate playback speed. (1/30th of a second)
            kernel_delay(1500000);
        }
    }

    // Restore terminal state
    terminal_fg_color = old_fg;
    terminal_bg_color = old_bg;
    clear_screen();
    print_string("CD+G player stopped.\n");
}