#include <stddef.h>
#include "shell.h"
#include "hdd_fs.h"
#include "basic.h"
#include "color.h"
#include "cdg_player.h"

#define BINARY_LOAD_ADDRESS 0x200000

// External kernel functions and variables
extern void print_string(const char *str);
extern void print_char(char c);
extern void print_int(int n);
extern void new_line(void);
extern size_t strlen(const char* str);
extern void clear_screen(void);
extern char* strcpy(char *dest, const char *src);
extern char* strcat(char *dest, const char *src);
extern int strcmp(const char *a, const char *b);
extern int strncmp(const char *a, const char *b, size_t n);
extern char input_buffer[];

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


// --- REWRITTEN AND CORRECTED PATH AND FS LOGIC ---

// Constructs a proper filesystem path from user input.
// Root files have no prefix (e.g., "hello.bin").
// Subdir files have a prefix (e.g., "bin/hello").
static void get_full_path(char* fullpath_out, const char* path_in) {
    // If path starts with '/', it's absolute from the root.
    if (path_in[0] == '/') {
        strcpy(fullpath_out, path_in + 1); // Copy, skipping the leading '/'
        return;
    }

    // Handle './' prefix by skipping it.
    if (path_in[0] == '.' && path_in[1] == '/') {
        path_in += 2;
    }

    // If CWD is root, the full path is just the relative path.
    if (strcmp(current_working_dir, "/") == 0) {
        strcpy(fullpath_out, path_in);
    } else {
        // If CWD is a subdir, construct "cwd/path".
        strcpy(fullpath_out, current_working_dir + 1); // Copy CWD without leading '/'
        strcat(fullpath_out, "/");
        strcat(fullpath_out, path_in);
    }
}

// A new, working `ls` command that filters by directory.
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
            // In root: find files with no slashes.
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
            // In a subdir (e.g., /bin): find files that start with "bin/"
            const char* cwd_prefix = current_working_dir + 1; // "bin"
            int prefix_len = strlen(cwd_prefix);
            if (strncmp(entry_name, cwd_prefix, prefix_len) == 0 && entry_name[prefix_len] == '/') {
                // Check that it's not in a sub-sub-directory
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
    strcat(full_path, "/"); // All directories must end in a slash
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
        for (int i = len - 1; i > 0; i--) {
            if (current_working_dir[i] == '/') {
                current_working_dir[i] = '\0';
                break;
            }
        }
    } else {
        char new_path[128];
        get_full_path(new_path, args);
        // We could check if dir exists, but for now we trust the user
        strcpy(current_working_dir, "/");
        strcat(current_working_dir, new_path);
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
        new_line(); print_string("System: help, ver, cls, echo, mr, color, info\n");
        print_string("FS:     ls, cd, md, read, write, format\n");
        print_string("Apps:   snake, basic, cdg\n");
        print_string("Run programs by path (e.g., /bin/prog or ./prog)\n");
    } else if (strcmp(command, "cls") == 0) {
        clear_screen();
    } else if (strcmp(command, "ls") == 0) {
        handle_ls(args);
    } else if (strcmp(command, "cd") == 0) {
        handle_cd(args);
    } else if (strcmp(command, "md") == 0) {
        handle_md(args);
    } else if (strcmp(command, "read") == 0) {
        new_line(); char p[128]; get_full_path(p, args);
        int bytes = fs_read_file(p, hdd_file_buffer);
        if (bytes >= 0) { print_string(hdd_file_buffer); new_line(); }
        else { print_string("Error reading file.\n"); }
    } else if (strcmp(command, "write") == 0) {
        new_line(); char* fn = args; char* data = NULL;
        for (int i = 0; args[i] != '\0'; i++) {
            if (args[i] == ' ') { args[i] = '\0'; data = &args[i+1]; break; }
        }
        if (data) { char p[128]; get_full_path(p, fn);
            if (fs_write_file(p, data, strlen(data))==0) print_string("OK\n"); else print_string("Error.\n");
        } else { print_string("Usage: write <file> <data>\n"); }
    }
    // ... other built-in commands like snake, basic, etc. can be placed here ...
    else {
        char full_path[128];
        get_full_path(full_path, command); // This now builds the correct path
        
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