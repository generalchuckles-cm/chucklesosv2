#include <stdint.h>
#include <stddef.h>
#include "shell.h"

// External functions from kernel.c
extern void print_string(const char *str);
extern void print_char(char c);
extern void print_int(int n);
extern void new_line(void);
extern void clear_screen(void);
extern void update_cursor(void);

// Port I/O functions (inline definitions)
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile ("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

// External VGA buffer and terminal state
extern unsigned short *terminal_buffer;
extern unsigned int vga_index;

// VGA Colors
#define BLACK_COLOR   0
#define GREEN_COLOR   2  // Snake color
#define RED_COLOR     4  // Apple color
#define BLUE_COLOR    1  // Score color
#define WHITE_COLOR   15

// Game constants
#define GAME_WIDTH    80
#define GAME_HEIGHT   25
#define MAX_SNAKE_LENGTH 100

// Game state
typedef struct {
    int x, y;
} Point;

Point snake[MAX_SNAKE_LENGTH];
int snake_length;
Point apple;
int score;
int game_running;
int direction; // 0=up, 1=right, 2=down, 3=left

// Direction vectors
int dx[] = {0, 1, 0, -1};
int dy[] = {-1, 0, 1, 0};

// Put colored character at position
void put_char_at(int x, int y, char c, int color) {
    if (x >= 0 && x < GAME_WIDTH && y >= 0 && y < GAME_HEIGHT) {
        terminal_buffer[y * GAME_WIDTH + x] = c | (color << 8);
    }
}

// Get character at position (for collision detection)
char get_char_at(int x, int y) {
    if (x >= 0 && x < GAME_WIDTH && y >= 0 && y < GAME_HEIGHT) {
        return terminal_buffer[y * GAME_WIDTH + x] & 0xFF;
    }
    return ' ';
}

// Simple random number generator
static uint32_t rng_state = 12345;
int simple_rand() {
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state >> 16) & 0x7FFF;
}

// Generate new apple position
void spawn_apple() {
    do {
        apple.x = simple_rand() % GAME_WIDTH;
        apple.y = simple_rand() % GAME_HEIGHT;
    } while (get_char_at(apple.x, apple.y) != ' '); // Don't spawn on snake
    
    put_char_at(apple.x, apple.y, 'A', RED_COLOR);
}

// Initialize game
void init_game() {
    clear_screen();
    
    // Initialize snake in center
    snake_length = 3;
    snake[0].x = GAME_WIDTH / 2;
    snake[0].y = GAME_HEIGHT / 2;
    snake[1].x = snake[0].x - 1;
    snake[1].y = snake[0].y;
    snake[2].x = snake[0].x - 2;
    snake[2].y = snake[0].y;
    
    direction = 1; // Start moving right
    score = 0;
    game_running = 1;
    
    // Draw initial snake
    for (int i = 0; i < snake_length; i++) {
        put_char_at(snake[i].x, snake[i].y, '#', GREEN_COLOR);
    }
    
    spawn_apple();
}

// Check if position is snake body
int is_snake_body(int x, int y) {
    for (int i = 0; i < snake_length; i++) {
        if (snake[i].x == x && snake[i].y == y) {
            return 1;
        }
    }
    return 0;
}

// Update game state
void update_game() {
    if (!game_running) return;
    
    // Calculate new head position
    Point new_head;
    new_head.x = snake[0].x + dx[direction];
    new_head.y = snake[0].y + dy[direction];
    
    // Check wall collision
    if (new_head.x < 0 || new_head.x >= GAME_WIDTH || 
        new_head.y < 0 || new_head.y >= GAME_HEIGHT) {
        game_running = 0;
        return;
    }
    
    // Check self collision
    if (is_snake_body(new_head.x, new_head.y)) {
        game_running = 0;
        return;
    }
    
    // Check apple collision
    int ate_apple = (new_head.x == apple.x && new_head.y == apple.y);
    
    if (!ate_apple) {
        // Remove tail
        Point tail = snake[snake_length - 1];
        put_char_at(tail.x, tail.y, ' ', BLACK_COLOR);
        
        // Move snake body
        for (int i = snake_length - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
    } else {
        // Snake grows, move body but don't remove tail
        for (int i = snake_length; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake_length++;
        score += 10;
        spawn_apple();
    }
    
    // Move head
    snake[0] = new_head;
    put_char_at(snake[0].x, snake[0].y, '#', GREEN_COLOR);
}

// Handle keyboard input
void handle_input() {
    if ((inb(0x64) & 1) == 0) return;
    uint8_t scancode = inb(0x60);
    
    // Only handle key press (not release)
    if (scancode & 0x80) return;
    
    int new_direction = direction;
    
    switch (scancode) {
        case 17: // W key
        case 72: // Up arrow
            if (direction != 2) new_direction = 0; // Up (don't reverse)
            break;
        case 31: // S key  
        case 80: // Down arrow
            if (direction != 0) new_direction = 2; // Down (don't reverse)
            break;
        case 30: // A key
        case 75: // Left arrow
            if (direction != 1) new_direction = 3; // Left (don't reverse)
            break;
        case 32: // D key
        case 77: // Right arrow
            if (direction != 3) new_direction = 1; // Right (don't reverse)
            break;
        case 1: // ESC key
            game_running = 0;
            break;
    }
    
    direction = new_direction;
}

// Simple delay function
void game_delay() {
    for (volatile int i = 0; i < 11000000; i++) {
        
    }
}

// Print score in blue and return to shell
void end_game() {
    clear_screen();
    vga_index = 0;
    
    print_string("Game Over!\n");
    
    // Print score in blue
    terminal_buffer[vga_index++] = 'S' | (BLUE_COLOR << 8);
    terminal_buffer[vga_index++] = 'c' | (BLUE_COLOR << 8);
    terminal_buffer[vga_index++] = 'o' | (BLUE_COLOR << 8);
    terminal_buffer[vga_index++] = 'r' | (BLUE_COLOR << 8);
    terminal_buffer[vga_index++] = 'e' | (BLUE_COLOR << 8);
    terminal_buffer[vga_index++] = ':' | (BLUE_COLOR << 8);
    terminal_buffer[vga_index++] = ' ' | (BLUE_COLOR << 8);
    
    // Print score number in blue
    char score_str[10];
    int temp_score = score;
    int digits = 0;
    
    if (temp_score == 0) {
        terminal_buffer[vga_index++] = '0' | (BLUE_COLOR << 8);
    } else {
        // Convert score to string
        while (temp_score > 0) {
            score_str[digits++] = (temp_score % 10) + '0';
            temp_score /= 10;
        }
        
        // Print digits in reverse order (correct order)
        for (int i = digits - 1; i >= 0; i--) {
            terminal_buffer[vga_index++] = score_str[i] | (BLUE_COLOR << 8);
        }
    }
    
    new_line();
    new_line();
    update_cursor();
}

// Main snake game function
void snake_game() {
    init_game();
    
    while (game_running) {
        handle_input();
        update_game();
        game_delay();
    }
    
    end_game();
}