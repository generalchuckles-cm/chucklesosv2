# ==== TOOLS ====
ASM       := nasm
CC        := gcc
LD        := ld
GRUB_MK   := grub-mkrescue

# ==== OUTPUT ====
ISO_NAME  := chucklesos2.iso
BUILD_DIR := build
ISO_DIR   := iso
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)/bin

# ==== COLOR ====
RED    := \033[1;31m
GREEN  := \033[1;32m
YELLOW := \033[1;33m
BLUE   := \033[1;34m
CYAN   := \033[1;36m
RESET  := \033[0m

# ==== SOURCES ====
C_SOURCES := kernel.c shell.c mem-read.c imfs.c imfscmd.c snake.c ata.c hdd_fs.c basic.c color.c ahci.c sata.c pci.c block.c cdg_player.c
C_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(C_SOURCES)))

ASM_SOURCES := boot.asm
ASM_OBJECTS := $(OBJ_DIR)/boot.o

KERNEL_ELF := $(BIN_DIR)/kernel

.PHONY: all clean

all: $(ISO_NAME)
	@echo "$(GREEN)Build finished successfully!$(RESET)"

# ==== COMPILE ====

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo -e "$(BLUE)Compiling $<$(RESET)"
	$(CC) -m32 -ffreestanding -O2 -c $< -o $@

$(OBJ_DIR)/boot.o: boot.asm | $(OBJ_DIR)
	@echo -e "$(BLUE)Assembling $<$(RESET)"
	$(ASM) -f elf32 $< -o $@

# ==== LINK ====

$(KERNEL_ELF): $(ASM_OBJECTS) $(C_OBJECTS) | $(BIN_DIR)
	@echo -e "$(YELLOW)Linking kernel$(RESET)"
	$(LD) -m elf_i386 -T linker.ld -o $@ $(ASM_OBJECTS) $(C_OBJECTS)

# ==== ISO ====

$(ISO_NAME): $(KERNEL_ELF) grub/grub.cfg | $(ISO_DIR)
	@echo -e "$(YELLOW)Creating ISO image$(RESET)"
	rm -rf $(ISO_DIR)/*
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB_MK) -o $@ $(ISO_DIR)

# ==== DIRECTORIES ====

$(OBJ_DIR) $(BIN_DIR) $(ISO_DIR):
	mkdir -p $@

# ==== CLEAN ====

clean:
	@echo -e "$(RED)Cleaning build artifacts and ISO$(RESET)"
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_NAME)