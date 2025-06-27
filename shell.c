#include <stddef.h>
#include "shell.h"
#include "hdd_fs.h"
#include "basic.h" // ADDED: Include header for the BASIC interpreter
#include "color.h" // ADDED: Include header for the color command
#include "cdg_player.h" // ADDED: Include header for the CDG player

// External print functions (defined in kernel.c)
extern void print_string(const char *str);
extern void print_char(char c);
extern void print_int(int n);
extern void new_line(void);
extern size_t strlen(const char* str);
extern void clear_screen(void); // ADDED: Extern for clear_screen

// Extern input buffer from kernel.c (now populated by get_user_input)
extern char input_buffer[];

// Buffer for reading file contents from the HDD
static char hdd_file_buffer[MAX_FILE_SIZE + 1];

// External function prototypes
extern void mem_read_command(const char *args);
extern void snake_game(void);
extern void basic_start(void); // ADDED: Extern for the BASIC interpreter's entry point
extern void handle_color_command(const char *args); // ADDED: Extern for color command
extern void cdg_player_start(const char *filename); // ADDED: Extern for CDG player

// IMFS command prototypes
void imfs_cmd_ls(const char *args);
void imfs_cmd_et(const char *args);
void imfs_cmd_cat(const char *args);

// Minimal strcmp/strncmp
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

void print_prompt() {
    print_string("guineapig$ ");
}

void process_command() {
    if (strlen(input_buffer) == 0) return;

    if (strcmp(input_buffer, "help") == 0) {
        new_line();
        print_string("--- ChucklesOS2 Commands ---\n");
        print_string("System: help, ver, cls, echo <text>, mr <hex_addr>, color <f/b> <0-15>, info\n");
        print_string("HDD:    hdd_ls, hdd_read <f>, hdd_write <f> <d>, hdd_format\n");
        print_string("Apps:   snake, basic, cdg <file.cdg>\n");
    } else if (strcmp(input_buffer, "info") == 0) {
        new_line();
        print_string("--- Chuckles OS v2 ---\n");
        print_string("Final Release of ChucklesOS is huge\n");
        print_string("Changelog:\n");
        print_string("Color command - See help. Change background/foreground color\n");
        print_string("Hard disk support - PATA/IDE and AHCI (like SATA) should work.\n");
        print_string("Snake Game\n");
        print_string("CD+G Player\n");
        print_string("Small Filesystem for disks\n");
        print_string("BASIC\n");
        print_string("Made in C\n");
        print_string("Copyleft 2025\n");
        print_string("\n");

    } else if (strcmp(input_buffer, "cls") == 0) {
        clear_screen();
    } else if (strcmp(input_buffer, "ver") == 0) {
        new_line();
        print_string("ChucklesOS v2.0\n");
    } else if (strncmp(input_buffer, "echo ", 5) == 0) {
        new_line();
        print_string(input_buffer + 5);
        new_line();
    } else if (strncmp(input_buffer, "mr ", 3) == 0) {
        mem_read_command(input_buffer + 3);
    } else if (strcmp(input_buffer, "ls") == 0) {
        imfs_cmd_ls(NULL);
    } else if (strncmp(input_buffer, "et ", 3) == 0) {
        imfs_cmd_et(input_buffer + 3);
    } else if (strncmp(input_buffer, "cat ", 4) == 0) {
        imfs_cmd_cat(input_buffer + 4);
    } else if (strcmp(input_buffer, "snake") == 0) {
        snake_game();
        clear_screen(); // Clear the game screen after exit
    }
    // --- HDD Driver Commands ---
    else if (strcmp(input_buffer, "hdd_ls") == 0) {
        new_line();
        fs_list_files();
    }
    else if (strcmp(input_buffer, "hdd_format") == 0) {
        new_line();
        print_string("This is a destructive action. For now, formatting happens immediately.\n");
        fs_format_disk();
    }
    else if (strncmp(input_buffer, "hdd_read ", 9) == 0) {
        new_line();
        char* filename = input_buffer + 9;
        int bytes_read = fs_read_file(filename, hdd_file_buffer);

        if (bytes_read >= 0) {
            print_string("--- Start of file: ");
            print_string(filename);
            print_string(" ---\n");
            print_string(hdd_file_buffer);
            print_string("\n--- End of file ---\n");
        } else {
            print_string("Error reading file.\n");
        }
    }
    else if (strncmp(input_buffer, "hdd_write ", 10) == 0) {
        new_line();
        char* args = input_buffer + 10;
        char* filename = args;
        char* data = NULL;

        for (int i = 0; args[i] != '\0'; i++) {
            if (args[i] == ' ') {
                args[i] = '\0';
                data = &args[i+1];
                break;
            }
        }
        if (data) {
            int result = fs_write_file(filename, data, strlen(data));
            if (result == 0) print_string("OK\n");
            else print_string("Error writing file.\n");
        } else {
            print_string("Usage: hdd_write <filename> <data>\n");
        }
    }
    
    // --- ADDED: Color Command ---
    else if (strncmp(input_buffer, "color ", 6) == 0) {
        handle_color_command(input_buffer + 6);
    }

    // --- ADDED: BASIC Interpreter Command ---
    else if (strcmp(input_buffer, "basic") == 0) {
        basic_start(); // This function call will not return until the user types EXIT in BASIC.
        // After returning, the screen is cleared and we give a message.
        print_string("Returned to ChucklesOS shell.\n");
    }

    // --- ADDED: CDG Player Command ---
    else if (strncmp(input_buffer, "cdg ", 4) == 0) {
        cdg_player_start(input_buffer + 4);
    }

    else {
        new_line();
        print_string("Unknown command. Type 'help' for a list of commands.\n");
    }
}