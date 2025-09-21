#ifndef HDD_FS_H
#define HDD_FS_H

#include <stdint.h>

#define MAX_FILENAME_LEN 32
#define MAX_FILES 12
#define MAX_FILE_SIZE (1024 * 1024 * 2)
#define HDD_SECTOR_SIZE 512
#define FS_LBA_OFFSET 30720

typedef struct {
    char filename[MAX_FILENAME_LEN];
    uint32_t start_lba;
    uint32_t size_bytes;
} __attribute__((packed)) FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    char padding[512 - (MAX_FILES * sizeof(FileEntry))];
} __attribute__((packed)) FileIndexTable;

// Make the file table globally accessible
extern FileIndexTable fs_table;

// Public Functions
void fs_init();
void fs_list_files();
int fs_read_file(const char* filename, char* buffer);
int fs_write_file(const char* filename, const char* data, uint32_t data_size);
void fs_format_disk();

#endif // HDD_FS_H