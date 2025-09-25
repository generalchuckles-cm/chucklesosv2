#include "cdg_player.h"
#include "hdd_fs.h"
#include "shell.h"
#include "ports.h"
#include "graphics.h" // For set_graphics_mode() and set_text_mode()
#include "extrainclude.h"
#include <stdint.h>
#include <stddef.h>

// Graphics mode constants
#define GFX_VIDEO_MEMORY    0xA0000
#define GFX_SCREEN_WIDTH    320
#define GFX_SCREEN_HEIGHT   200

// CD+G native resolution constants
#define CDG_WIDTH   300
#define CDG_HEIGHT  216
#define CDG_TILE_W   6
#define CDG_TILE_H   12
#define CDG_COLS (CDG_WIDTH / CDG_TILE_W)  // 50
#define CDG_ROWS (CDG_HEIGHT / CDG_TILE_H) // 18

// Centering offset for the 300px wide image on a 320px screen
#define X_OFFSET ((GFX_SCREEN_WIDTH - CDG_WIDTH) / 2) // = 10

// CDG Packet Constants
#define CDG_PACKET_SIZE      24
#define CDG_COMMAND_MASK     0x3F
#define CDG_COMMAND          0x09
#define CDG_INSTR_MEM_PRESET      1
#define CDG_INSTR_BORDER_PRESET   2
#define CDG_INSTR_TILE_BLOCK      6
#define CDG_INSTR_TILE_BLOCK_XOR  38
#define CDG_INSTR_LOAD_CLUT_LOW   30
#define CDG_INSTR_LOAD_CLUT_HIGH  31

#define MAX_CDG_FILE_SIZE (CDG_PACKET_SIZE * 30000)
static uint8_t file_buffer[MAX_CDG_FILE_SIZE];

// --- VGA DAC (Palette) Programming ---
static void program_dac_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    outb(0x3C8, index);
    // CDG provides 4-bit color (0-15). VGA DAC expects 6-bit (0-63). Scale by 4.
    outb(0x3C9, r * 4);
    outb(0x3C9, g * 4);
    outb(0x3C9, b * 4);
}

// --- New Graphical Packet Handler ---
static void handle_g_packet(const uint8_t* packet) {
    uint8_t command = packet[0] & CDG_COMMAND_MASK;
    uint8_t instr   = packet[1] & CDG_COMMAND_MASK;
    const uint8_t* data = &packet[4];

    if (command != CDG_COMMAND) return;

    volatile uint8_t* vram = (uint8_t*)GFX_VIDEO_MEMORY;

    switch (instr) {
        case CDG_INSTR_MEM_PRESET: {
            uint8_t color = data[0] & 0x0F;
            memset((void*)vram, color, GFX_SCREEN_WIDTH * GFX_SCREEN_HEIGHT);
            break;
        }

        case CDG_INSTR_BORDER_PRESET: {
            uint8_t color = data[0] & 0x0F;
            for (int y = 0; y < GFX_SCREEN_HEIGHT; y++) {
                for (int x = 0; x < X_OFFSET; x++) vram[y * GFX_SCREEN_WIDTH + x] = color;
                for (int x = X_OFFSET + CDG_WIDTH; x < GFX_SCREEN_WIDTH; x++) vram[y * GFX_SCREEN_WIDTH + x] = color;
            }
            break;
        }

        case CDG_INSTR_TILE_BLOCK:
        case CDG_INSTR_TILE_BLOCK_XOR: {
            uint8_t color0 = data[0] & 0x0F;
            uint8_t color1 = data[1] & 0x0F;
            uint8_t row = data[2] & 0x1F;
            uint8_t col = data[3] & 0x3F;

            if (row >= CDG_ROWS || col >= CDG_COLS) return;

            int source_start_x = col * CDG_TILE_W;
            int source_start_y = row * CDG_TILE_H;

            for (int y_tile = 0; y_tile < CDG_TILE_H; y_tile++) {
                uint8_t pixel_data = data[4 + y_tile];
                int source_y = source_start_y + y_tile;
                int target_y = (source_y * GFX_SCREEN_HEIGHT) / CDG_HEIGHT;

                if (target_y >= GFX_SCREEN_HEIGHT) continue;

                for (int x_tile = 0; x_tile < CDG_TILE_W; x_tile++) {
                    int target_x = source_start_x + x_tile + X_OFFSET;
                    if (target_x >= GFX_SCREEN_WIDTH) continue;

                    uint32_t offset = target_y * GFX_SCREEN_WIDTH + target_x;
                    uint8_t pixel_color = ((pixel_data >> (5 - x_tile)) & 1) ? color1 : color0;
                    
                    if (instr == CDG_INSTR_TILE_BLOCK_XOR) {
                        vram[offset] ^= pixel_color;
                    } else {
                        vram[offset] = pixel_color;
                    }
                }
            }
            break;
        }

        case CDG_INSTR_LOAD_CLUT_LOW:
        case CDG_INSTR_LOAD_CLUT_HIGH: {
            int start_index = (instr == CDG_INSTR_LOAD_CLUT_LOW) ? 0 : 8;
            for (int i = 0; i < 8; i++) {
                const uint8_t* color_data = &data[i * 2];
                uint8_t r = (color_data[0] >> 2) & 0x0F;
                uint8_t g = ((color_data[0] & 0x03) << 2) | ((color_data[1] >> 4) & 0x03);
                uint8_t b = color_data[1] & 0x0F;
                program_dac_color(start_index + i, r, g, b);
            }
            break;
        }

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

    print_string("Switching to graphics mode... Press ESC to exit.\n");
    kernel_delay(50000000);

    set_graphics_mode();
    memset((void*)GFX_VIDEO_MEMORY, 0, GFX_SCREEN_WIDTH * GFX_SCREEN_HEIGHT);

    int total_packets = bytes_read / CDG_PACKET_SIZE;
    int packet_batch_size = 25;

    for (int i = 0; i < total_packets; i++) {
        if ((inb(0x64) & 1) && inb(0x60) == 1) {
            break;
        }

        const uint8_t* packet = &file_buffer[i * CDG_PACKET_SIZE];
        handle_g_packet(packet);

        if (i % packet_batch_size == 0) {
            kernel_delay(1500000);
        }
    }

    set_text_mode();
    clear_screen();
    print_string("CD+G player stopped.\n");
}