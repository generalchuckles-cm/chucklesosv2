#ifndef HDD_FS_H
#define HDD_FS_H

#include <stdint.h>

#define MAX_FILENAME_LEN 32
#define MAX_FILES 12 // 512-byte sector / 40 bytes per entry = 12.8 -> 12 files
#define MAX_FILE_SIZE (1024 * 1024 * 2) // Max file size of 2MB
#define HDD_SECTOR_SIZE 512

typedef struct {
    char filename[MAX_FILENAME_LEN];
    uint32_t start_lba;
    uint32_t size_bytes;
} __attribute__((packed)) FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    char padding[512 - (MAX_FILES * sizeof(FileEntry))]; // Pad to 512 bytes
} __attribute__((packed)) FileIndexTable;

// Public Functions
void fs_init();
void fs_list_files();
int fs_read_file(const char* filename, char* buffer);
int fs_write_file(const char* filename, const char* data, uint32_t data_size);
void fs_format_disk(); // A utility to wipe the file index

#endif // HDD_FS_H