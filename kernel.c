#include <stdint.h>
#include <stddef.h>
#include "shell.h"
#include "ports.h"
#include "block.h" // Include the new block layer
#include "hdd_fs.h"

char *strcpy(char *dest, const char *src) {
    char *original_dest = dest;
    while ((*dest++ = *src++));
    return original_dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

void* memset(void *s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while(n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

// ==== CONSTANTS ====
#define VGA_ADDRESS 0xB8000
#define MAX_COLS 80
#define MAX_ROWS 25
#define INPUT_BUFFER_SIZE 128

// ==== GLOBALS ====
unsigned short *terminal_buffer;
unsigned int vga_index = 0;
int shift = 0;
uint8_t terminal_fg_color = 15; // White
uint8_t terminal_bg_color = 0;  // Black
char input_buffer[INPUT_BUFFER_SIZE];

// ==== PORT I/O ====
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile ("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}
void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ==== TERMINAL ====

void update_screen_colors() {
    uint8_t attribute = (terminal_bg_color << 4) | (terminal_fg_color & 0x0F);
    for (int i = 0; i < MAX_COLS * MAX_ROWS; i++) {
        terminal_buffer[i] = (terminal_buffer[i] & 0x00FF) | (attribute << 8);
    }
}

void update_cursor() {
    uint16_t pos = vga_index;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}
void clear_screen() {
    uint8_t attribute = (terminal_bg_color << 4) | (terminal_fg_color & 0x0F);
    unsigned short blank = ' ' | (attribute << 8);
    for (int i = 0; i < MAX_COLS * MAX_ROWS; i++) {
        terminal_buffer[i] = blank;
    }
    vga_index = 0;
    update_cursor();
}
void scroll_up() {
    for (int row = 1; row < MAX_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            terminal_buffer[(row - 1) * MAX_COLS + col] = terminal_buffer[row * MAX_COLS + col];
        }
    }
    uint8_t attribute = (terminal_bg_color << 4) | (terminal_fg_color & 0x0F);
    unsigned short blank = ' ' | (attribute << 8);
    for (int col = 0; col < MAX_COLS; col++) {
        terminal_buffer[(MAX_ROWS - 1) * MAX_COLS + col] = blank;
    }
}
void new_line() {
    vga_index = ((vga_index / MAX_COLS) + 1) * MAX_COLS;
    if (vga_index >= MAX_COLS * MAX_ROWS) {
        scroll_up();
        vga_index = (MAX_ROWS - 1) * MAX_COLS;
    }
    update_cursor();
}

void backspace_vga() {
    if (vga_index > 0 && (vga_index % MAX_COLS) > 0) {
        vga_index--;
        uint8_t attribute = (terminal_bg_color << 4) | (terminal_fg_color & 0x0F);
        terminal_buffer[vga_index] = ' ' | (attribute << 8);
        update_cursor();
    }
}
void print_char(char c) {
    if (vga_index >= MAX_COLS * MAX_ROWS) {
       new_line();
    }
    uint8_t attribute = (terminal_bg_color << 4) | (terminal_fg_color & 0x0F);
    terminal_buffer[vga_index++] = c | (attribute << 8);
    update_cursor();
}
void print_string(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') new_line();
        else print_char(str[i]);
    }
}
void print_int(int n) {
    if (n == 0) {
        print_char('0');
        return;
    }
    char buf[12];
    int i = 0;
    if (n < 0) {
        print_char('-');
        n = -n;
    }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}

void print_hex(uint32_t val) {
    const char *hex_digits = "0123456789ABCDEF";
    print_string("0x");
    for (int i = 7; i >= 0; i--) {
        print_char(hex_digits[(val >> (i * 4)) & 0xF]);
    }
}

// ==== KEYBOARD ====
char scancode_map[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
};
char scancode_shift[128] = {
    0, 27,'!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
};

char get_single_keypress() {
    while (1) {
        while ((inb(0x64) & 1) == 0) {}
        uint8_t scancode = inb(0x60);
        if (scancode & 0x80) {
            scancode &= 0x7F;
            if (scancode == 42 || scancode == 54) shift = 0;
        } else {
            if (scancode == 42 || scancode == 54) {
                shift = 1;
            } else {
                char c = shift ? scancode_shift[scancode] : scancode_map[scancode];
                if (c) {
                    return c;
                }
            }
        }
    }
}

void get_user_input(char* buffer, int max_len) {
    int pos = 0;
    memset(buffer, 0, max_len);

    while (1) {
        char c = get_single_keypress();
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                backspace_vga();
            }
        } else if (c == '\n') {
            buffer[pos] = '\0';
            new_line();
            return;
        } else {
            if (pos < max_len - 1) {
                buffer[pos++] = c;
                print_char(c);
            }
        }
    }
}

// ==== NEW FUNCTION FOR TIMING ====
void kernel_delay(uint32_t cycles) {
    for (volatile uint32_t i = 0; i < cycles; i++) {
        // This is a simple busy-wait loop.
        // The value of 'cycles' needs to be tuned for the target machine speed.
    }
}

// ==== MAIN ====
void main(void) {
    terminal_buffer = (unsigned short *)VGA_ADDRESS;
    clear_screen();

    print_string("ChucklesOS2 booting...\n");
    block_init(); // This now handles ATA and SATA probing
    fs_init();
    new_line();

    while (1) {
        print_prompt();
        get_user_input(input_buffer, INPUT_BUFFER_SIZE);
        process_command();
    }
}