#include <string.h>
#include "shell.h"

// Extern IMFS API
void imfs_list_files();
void imfs_write_file(const char *filename, const char *content);
const char* imfs_read_file(const char *filename);

extern void print_string(const char *str);
extern void print_char(char c);
extern void new_line(void);

// Helper: parses et "some text" filename.txt
// Returns 1 if successful, 0 otherwise
static int parse_et_args(const char *input, char *text_out, size_t text_size, char *filename_out, size_t filename_size) {
    size_t i = 0;

    // Skip leading spaces
    while (input[i] == ' ') i++;

    // Must start with a quote
    if (input[i] != '"') return 0;
    i++; // skip opening quote

    // Capture everything until closing quote or buffer full
    size_t j = 0;
    while (input[i] && input[i] != '"' && j + 1 < text_size) {
        text_out[j++] = input[i++];
    }
    if (input[i] != '"') return 0; // no closing quote found
    text_out[j] = '\0';
    i++; // skip closing quote

    // Skip spaces before filename
    while (input[i] == ' ') i++;

    // Capture filename until space or end or buffer full
    j = 0;
    while (input[i] && input[i] != ' ' && j + 1 < filename_size) {
        filename_out[j++] = input[i++];
    }
    filename_out[j] = '\0';

    if (filename_out[0] == '\0') return 0; // filename missing

    return 1;
}

// Command: ls
void imfs_cmd_ls(const char *args) {
    (void)args;
    imfs_list_files();
}

// Command: et "text" filename.txt
void imfs_cmd_et(const char *args) {
    char text[128] = {0};
    char filename[64] = {0};

    if (!parse_et_args(args, text, sizeof(text), filename, sizeof(filename))) {
        print_string("Usage: et \"text...\" filename\n");
        new_line();
        return;
    }

    imfs_write_file(filename, text);
    print_string("File saved: ");
    print_string(filename);
    new_line();
}

// Command: cat filename.txt
void imfs_cmd_cat(const char *args) {
    char filename[64] = {0};
    // Skip spaces
    size_t i = 0, j = 0;
    while (args[i] == ' ') i++;
    while (args[i] && args[i] != ' ' && args[i] != '\n') {
        filename[j++] = args[i++];
    }
    filename[j] = '\0';

    if (filename[0] == '\0') {
        print_string("Usage: cat filename\n");
        new_line();
        return;
    }

    const char *content = imfs_read_file(filename);
    if (content == NULL) {
        print_string("File not found\n");
        new_line();
    } else {
        print_string(content);
        new_line();
    }
}
