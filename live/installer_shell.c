#include "shell.h"
#include "hdd_fs.h"
#include "block.h"
#include <stddef.h>
#include <stdint.h>

extern void print_string(const char *str);
extern void print_int(int n);
extern void new_line(void);
extern void clear_screen(void);
extern void get_user_input(char* buffer, int max_len);
extern void* memset(void* s, int c, size_t n);
extern int strcmp(const char *a, const char *b);
extern size_t strlen(const char* str);

static inline void hlt(void) {
    __asm__ volatile ("hlt");
}

extern const uint8_t os_image_start[];
extern const uint8_t os_image_end[];
extern char input_buffer[];

static void handle_install_command() {
    char confirm_buffer[10];

    new_line();
    print_string("--- ChucklesOS Installer ---\n\n");
    print_string("WARNING: This will erase all data on the primary hard disk.\n");
    print_string("Type 'YES' to continue: ");
    get_user_input(confirm_buffer, 10);

    if (strcmp(confirm_buffer, "YES") != 0) {
        print_string("Installation aborted.\n");
        return;
    }

    if (!block_device_available) {
        print_string("Error: No hard disk detected. Cannot install.\n");
        return;
    }

    print_string("Writing bootable disk image... ");
    const uint8_t* image_ptr = os_image_start;
    size_t image_size = os_image_end - os_image_start;
    uint32_t sectors_to_write = (image_size + HDD_SECTOR_SIZE - 1) / HDD_SECTOR_SIZE;

    if (sectors_to_write > FS_LBA_OFFSET) {
        print_string("FATAL ERROR!\n");
        return;
    }

    for (uint32_t i = 0; i < sectors_to_write; i++) {
        if (block_write(i, 1, (void*)(image_ptr + (i * HDD_SECTOR_SIZE))) != 0) {
            print_string("FAILED.\n");
            return;
        }
    }
    print_string("OK\n");

    print_string("Creating data partition... ");
    fs_format_disk();

    // *** ADDED EXPLICIT ERROR CHECKING ***
    print_string("Creating /bin/ directory... ");
    if (fs_write_file("/bin/", "", 0) != 0) {
        print_string("**FAILED**\n");
    } else {
        print_string("OK\n");
    }

    print_string("Creating /user/ directory... ");
    if (fs_write_file("/user/", "", 0) != 0) {
        print_string("**FAILED**\n");
    } else {
        print_string("OK\n");
    }

    new_line();
    print_string("--- Installation Complete! ---\n");
    print_string("You can now reboot.\n");
    
    hlt();
}

void print_prompt() {
    print_string("installer# ");
}

void process_command() {
    if (strlen(input_buffer) == 0) return;
    if (strcmp(input_buffer, "help") == 0) {
        new_line();
        print_string("  install - Start the installation process.\n");
        print_string("  help    - Show this message.\n");
    } else if (strcmp(input_buffer, "install") == 0) {
        handle_install_command();
    } else {
        new_line();
        print_string("Unknown command. Type 'help'.\n");
    }
}