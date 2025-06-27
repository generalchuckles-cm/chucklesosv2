#include <string.h>
#include "shell.h"

#define MAX_FILES 32
#define MAX_FILENAME_LEN 32
#define MAX_FILE_CONTENT_LEN 512

typedef struct {
    char filename[MAX_FILENAME_LEN];
    char content[MAX_FILE_CONTENT_LEN];
    int used;
} IMFS_File;

static IMFS_File files[MAX_FILES];

// Initialize filesystem
void imfs_init() {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
        files[i].filename[0] = '\0';
        files[i].content[0] = '\0';
    }
}

// Create or overwrite file
void imfs_write_file(const char *filename, const char *content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].filename, filename) == 0) {
            strncpy(files[i].content, content, MAX_FILE_CONTENT_LEN - 1);
            files[i].content[MAX_FILE_CONTENT_LEN - 1] = '\0';
            return;
        }
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            strncpy(files[i].filename, filename, MAX_FILENAME_LEN - 1);
            files[i].filename[MAX_FILENAME_LEN - 1] = '\0';
            strncpy(files[i].content, content, MAX_FILE_CONTENT_LEN - 1);
            files[i].content[MAX_FILE_CONTENT_LEN - 1] = '\0';
            return;
        }
    }
    // No space, ignore for now or print error
}

// Read file content, returns NULL if not found
const char* imfs_read_file(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].filename, filename) == 0) {
            return files[i].content;
        }
    }
    return NULL;
}

// List all filenames
void imfs_list_files() {
    int any = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            print_string(files[i].filename);
            new_line();
            any = 1;
        }
    }
    if (!any) {
        print_string("(No files)");
        new_line();
    }
}
