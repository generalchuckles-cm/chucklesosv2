#include "hdd_fs.h"
#include "block.h"
#include <stddef.h>
#include "shell.h"

// Extern declarations
extern void print_string(const char* str);
extern void print_int(int n);
extern void new_line();
extern char* strncpy(char *dest, const char *src, size_t n);
extern int strcmp(const char *a, const char *b);
extern size_t strlen(const char* str);
extern void* memset(void *s, int c, size_t n);
extern void print_char(char c);

// *** CORRECTED: Removed 'static' to make this a global definition ***
FileIndexTable fs_table;
static uint32_t next_free_lba;

void fs_init() {
    if (!block_device_available) {
        print_string("HDD FS: Skipping init, no block device available.\n");
        return;
    }
    if (block_read(FS_LBA_OFFSET, 1, &fs_table) != 0) {
        print_string("HDD FS: Error reading File Index Table. Disabling FS.\n");
        block_device_available = 0;
        return;
    }
    next_free_lba = 1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_table.entries[i].filename[0] != '\0') {
            uint32_t num_sectors = (fs_table.entries[i].size_bytes + HDD_SECTOR_SIZE - 1) / HDD_SECTOR_SIZE;
            uint32_t file_end_lba = fs_table.entries[i].start_lba + num_sectors;
            if (file_end_lba > next_free_lba) {
                next_free_lba = file_end_lba;
            }
        }
    }
    print_string("HDD FS Initialized. Partition starts at LBA ");
    print_int(FS_LBA_OFFSET);
    print_string(".\n");
}

void fs_format_disk() {
    if (!block_device_available) {
        print_string("Error: No block device available.\n");
        return;
    }
    print_string("Formatting data partition... ");
    memset(&fs_table, 0, sizeof(FileIndexTable));
    if (block_write(FS_LBA_OFFSET, 1, &fs_table) != 0) {
        print_string("Error: Failed to write new FIT to disk.\n");
        return;
    }
    fs_init();
    print_string("Done.\n");
}

void fs_list_files() {
    if (!block_device_available) return;
    print_string("--- HDD File Listing ---\n");
    print_string("Type | Name                           | Size (Bytes)\n");
    print_string("----------------------------------------------------\n");
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_table.entries[i].filename[0] != '\0') {
            int len = strlen(fs_table.entries[i].filename);
            if (fs_table.entries[i].filename[len - 1] == '/') {
                print_string("[d]  | ");
            } else {
                print_string("[f]  | ");
            }
            print_string(fs_table.entries[i].filename);
            for (int p = len; p < 30; p++) print_char(' ');
            print_string(" | ");
            print_int(fs_table.entries[i].size_bytes);
            new_line();
            count++;
        }
    }
    if (count == 0) print_string("(No files found)\n");
}

int fs_read_file(const char* filename, char* buffer) {
    if (!block_device_available) return -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(fs_table.entries[i].filename, filename) == 0) {
            FileEntry* entry = &fs_table.entries[i];
            if (entry->size_bytes > MAX_FILE_SIZE) return -2;
            uint32_t num_sectors = (entry->size_bytes + HDD_SECTOR_SIZE - 1) / HDD_SECTOR_SIZE;
            if (block_read(entry->start_lba + FS_LBA_OFFSET, num_sectors, buffer) != 0) return -1;
            buffer[entry->size_bytes] = '\0';
            return entry->size_bytes;
        }
    }
    return -1;
}

int fs_write_file(const char* filename, const char* data, uint32_t data_size) {
    if (!block_device_available) return -1;
    if (strlen(filename) >= MAX_FILENAME_LEN) {
        print_string("Error: Filename too long.\n");
        return -4;
    }
    if (data_size > MAX_FILE_SIZE) return -2;
    int free_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(fs_table.entries[i].filename, filename) == 0) {
            print_string("Error: File already exists.\n");
            return -5;
        }
        if (free_index == -1 && fs_table.entries[i].filename[0] == '\0') {
            free_index = i;
        }
    }
    if (free_index == -1) {
        print_string("Error: File table is full.\n");
        return -3;
    }
    uint32_t num_sectors = (data_size + HDD_SECTOR_SIZE - 1) / HDD_SECTOR_SIZE;
    if (block_write(next_free_lba + FS_LBA_OFFSET, num_sectors, data) != 0) return -1;
    FileEntry* new_entry = &fs_table.entries[free_index];
    strncpy(new_entry->filename, filename, MAX_FILENAME_LEN - 1);
    new_entry->filename[MAX_FILENAME_LEN - 1] = '\0';
    new_entry->start_lba = next_free_lba;
    new_entry->size_bytes = data_size;
    if (block_write(FS_LBA_OFFSET, 1, &fs_table) != 0) return -1;
    next_free_lba += num_sectors;
    return 0;
}