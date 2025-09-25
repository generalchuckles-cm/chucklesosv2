#include <stddef.h>
#include "shell.h"
#include "hdd_fs.h"
#include "basic.h"
#include "color.h"
#include "cdg_player.h"
#include "graphics.h"

#define BINARY_LOAD_ADDRESS 0x200000

// External kernel variables
extern char input_buffer[];
extern int IsGraphics; // Get access to the new global state flag

// Extern the file table so `ls` can inspect it directly
extern FileIndexTable fs_table;

static char hdd_file_buffer[MAX_FILE_SIZE + 1];
static char current_working_dir[128] = "/";

// External function prototypes
extern void mem_read_command(const char *args);
extern void snake_game(void);
extern void basic_start(void);
extern void handle_color_command(const char *args);
extern void cdg_player_start(const char *filename);


// --- Path and FS Logic ---

static void get_full_path(char* fullpath_out, const char* path_in) {
    if (path_in[0] == '/') {
        strcpy(fullpath_out, path_in + 1);
        return;
    }
    if (path_in[0] == '.' && path_in[1] == '/') {
        path_in += 2;
    }
    if (strcmp(current_working_dir, "/") == 0) {
        strcpy(fullpath_out, path_in);
    } else {
        strcpy(fullpath_out, current_working_dir + 1);
        strcat(fullpath_out, "/");
        strcat(fullpath_out, path_in);
    }
}

static void handle_ls(const char* args) {
    print_string("--- Listing for ");
    print_string(current_working_dir);
    print_string(" ---\n");
    print_string("Type | Name\n");
    print_string("-------------------------\n");

    int count = 0;
    int cwd_len = strlen(current_working_dir);

    for (int i = 0; i < MAX_FILES; i++) {
        const char* entry_name = fs_table.entries[i].filename;
        if (entry_name[0] == '\0') continue;

        const char* name_to_print = NULL;

        if (strcmp(current_working_dir, "/") == 0) {
            int has_slash = 0;
            for (int j = 0; entry_name[j] != '\0'; j++) {
                if (entry_name[j] == '/') {
                    has_slash = 1;
                    break;
                }
            }
            if (!has_slash) {
                name_to_print = entry_name;
            }
        } else {
            const char* cwd_prefix = current_working_dir + 1;
            int prefix_len = strlen(cwd_prefix);
            if (strncmp(entry_name, cwd_prefix, prefix_len) == 0 && entry_name[prefix_len] == '/') {
                const char* remainder = entry_name + prefix_len + 1;
                int has_more_slashes = 0;
                for (int j = 0; remainder[j] != '\0'; j++) {
                    if (remainder[j] == '/') {
                        has_more_slashes = 1;
                        break;
                    }
                }
                if (!has_more_slashes) {
                    name_to_print = remainder;
                }
            }
        }

        if (name_to_print) {
            int len = strlen(entry_name);
            if (entry_name[len - 1] == '/') {
                print_string("[d]  | ");
            } else {
                print_string("[f]  | ");
            }
            print_string(name_to_print);
            new_line();
            count++;
        }
    }

    if (count == 0) print_string("(Directory is empty)\n");
}

static void handle_md(const char* args) {
    if (strlen(args) == 0) {
        print_string("Usage: md <directory_name>\n");
        return;
    }
    char full_path[128];
    get_full_path(full_path, args);
    strcat(full_path, "/");
    if (fs_write_file(full_path, "", 0) != 0) {
        print_string("Error creating directory.\n");
    }
}

static void handle_cd(const char* args) {
    if (strlen(args) == 0) return;
    if (strcmp(args, "/") == 0) {
        strcpy(current_working_dir, "/");
        return;
    }
    if (strcmp(args, "..") == 0) {
        if (strcmp(current_working_dir, "/") == 0) return;
        int len = strlen(current_working_dir);
        // Go back to the last slash
        for (int i = len - 2; i > 0; i--) {
            if (current_working_dir[i] == '/') {
                current_working_dir[i + 1] = '\0';
                return;
            }
        }
        // If no other slash found, go to root
        strcpy(current_working_dir, "/");
    } else {
        char new_path[128];
        get_full_path(new_path, args);
        
        // We need to verify the directory exists.
        // We do this by checking if a file entry for "new_path/" exists.
        char temp_path[130];
        strcpy(temp_path, new_path);
        strcat(temp_path, "/");
        
        int found = 0;
        for (int i = 0; i < MAX_FILES; i++) {
            if (strcmp(fs_table.entries[i].filename, temp_path) == 0) {
                found = 1;
                break;
            }
        }

        if (found) {
            strcpy(current_working_dir, "/");
            strcat(current_working_dir, new_path);
        } else {
            print_string("Directory not found: ");
            print_string(args);
            new_line();
        }
    }
}

void print_prompt() {
    print_string("guineapig:");
    print_string(current_working_dir);
    print_string("$ ");
}

void process_command() {
    if (strlen(input_buffer) == 0) return;
    char* command = input_buffer;
    char* args = "";
    for (int i = 0; i < strlen(input_buffer); i++) {
        if (input_buffer[i] == ' ') { input_buffer[i] = '\0'; args = &input_buffer[i+1]; break; }
    }

    if (strcmp(command, "help") == 0) {
        new_line();
        print_string("System: help, cls, mr, color, graphics, textmode\n");
        print_string("FS:     ls, cd, md, read, write, format\n");
        print_string("Apps:   snake, basic, cdg (graphical)\n");
    } else if (strcmp(command, "cls") == 0) {
        clear_screen();
    } else if (strcmp(command, "graphics") == 0) {
        if (IsGraphics) {
            print_string("Already in graphics mode.\n");
        } else {
            set_graphics_mode();
            IsGraphics = 1;
            g_init();
            print_string("Switched to graphics mode shell.\n");
            print_string("Type 'textmode' to return.\n");
        }
    } else if (strcmp(command, "textmode") == 0) {
        if (!IsGraphics) {
            print_string("Already in text mode.\n");
        } else {
            set_text_mode();
            IsGraphics = 0;
            clear_screen();
            print_string("Switched to legacy text mode.\n");
        }
    } else if (strcmp(command, "ls") == 0) {
        handle_ls(args);
    } else if (strcmp(command, "cd") == 0) {
        handle_cd(args);
    } else if (strcmp(command, "md") == 0 || strcmp(command, "mkdir") == 0) {
        handle_md(args);
    } else if (strcmp(command, "read") == 0 || strcmp(command, "cat") == 0) {
        new_line(); char p[128]; get_full_path(p, args);
        int bytes = fs_read_file(p, hdd_file_buffer);
        if (bytes >= 0) { print_string(hdd_file_buffer); new_line(); }
        else { print_string("Error reading file.\n"); }
    } else if (strcmp(command, "write") == 0 || strcmp(command, "wr") == 0) {
        new_line(); char* fn = args; char* data = NULL;
        for (int i = 0; args[i] != '\0'; i++) {
            if (args[i] == ' ') { args[i] = '\0'; data = &args[i+1]; break; }
        }
        if (data) { char p[128]; get_full_path(p, fn);
            if (fs_write_file(p, data, strlen(data))==0) print_string("OK\n"); else print_string("Error.\n");
        } else { print_string("Usage: write <file> <data>\n"); }
    } else if (strcmp(command, "format") == 0) {
        fs_format_disk();
    } else if (strcmp(command, "snake") == 0) {
        snake_game();
    } else if (strcmp(command, "basic") == 0) {
        basic_start();
    } else if (strcmp(command, "color") == 0) {
        handle_color_command(args);
    } else if (strcmp(command, "mr") == 0) {
        mem_read_command(args);
    } else if (strcmp(command, "cdg") == 0) {
        if (*args == '\0') {
            print_string("Usage: cdg <filename>\n");
        } else {
            char p[128];
            get_full_path(p, args);
            cdg_player_start(p);
        }
    } else {
        char full_path[128];
        get_full_path(full_path, command);
        
        int bytes_read = fs_read_file(full_path, (char*)BINARY_LOAD_ADDRESS);

        if (bytes_read > 0) {
            void (*app)(void) = (void*)BINARY_LOAD_ADDRESS;
            app();
        } else {
            new_line();
            print_string("Unknown command or program not found: ");
            print_string(command);
            new_line();
        }
    }
}