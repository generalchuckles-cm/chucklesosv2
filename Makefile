# ==== TOOLS ====
ASM       := nasm
CC        := gcc
LD        := ld
OBJCOPY   := objcopy
GRUB_MK   := grub-mkrescue

# ==== OUTPUT ====
BUILD_DIR     := build
OBJ_DIR       := $(BUILD_DIR)/obj
BIN_DIR       := $(BUILD_DIR)/bin

# --- Staging Directories ---
MNT_DIR           := $(BUILD_DIR)/mnt
INSTALLER_ISO_DIR := $(BUILD_DIR)/iso_installer

# --- Final Output Files ---
FINAL_OS_HDD_IMG  := $(BUILD_DIR)/chucklesos_final.img
INSTALLER_ISO     := chucklesos_installer.iso

# ==== KERNELS ====
FINAL_OS_ELF      := $(BIN_DIR)/kernel.elf
INSTALLER_ELF     := $(BIN_DIR)/installer_kernel.elf

# ==== COMPILER FLAGS ====
CFLAGS        := -m32 -ffreestanding -O2 -I.

# ==== COLOR ====
GREEN  := \033[1;32m
YELLOW := \033[1;33m
BLUE   := \033[1;34m
CYAN   := \033[1;36m
RED    := \033[1;31m

# ==== SOURCES ====
COMMON_C_SOURCES := \
    kernel.c mem-read.c snake.c ata.c hdd_fs.c basic.c color.c \
    ahci.c sata.c pci.c block.c cdg_player.c

OS_ONLY_SOURCES        := shell.c
INSTALLER_ONLY_SOURCES := live/installer_shell.c

COMMON_OBJECTS        := $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(COMMON_C_SOURCES)))
OS_C_OBJECTS          := $(COMMON_OBJECTS) $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(OS_ONLY_SOURCES)))
OS_ASM_OBJECTS        := $(OBJ_DIR)/boot.o
INSTALLER_C_OBJECTS   := $(COMMON_OBJECTS) $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(INSTALLER_ONLY_SOURCES)))
INSTALLER_ASM_OBJECTS := $(OBJ_DIR)/boot.o $(OBJ_DIR)/os_image.o

# ==============================================================================
# --- BUILD TARGETS ---
# ==============================================================================

.PHONY: all clean

all: $(INSTALLER_ISO)
	@echo "$(GREEN)Build finished successfully! Installer ISO is ready: $(INSTALLER_ISO)$(RESET)"

# --- Stage 4: Create the Final Installer ISO ---
$(INSTALLER_ISO): $(INSTALLER_ELF) grub/grub.cfg | $(INSTALLER_ISO_DIR)
	@echo -e "$(YELLOW)Creating Installer ISO image...$(RESET)"
	rm -rf $(INSTALLER_ISO_DIR)/*
	mkdir -p $(INSTALLER_ISO_DIR)/boot/grub
	cp $(INSTALLER_ELF) $(INSTALLER_ISO_DIR)/boot/kernel
	cp grub/grub.cfg $(INSTALLER_ISO_DIR)/boot/grub/
	$(GRUB_MK) -o $@ $(INSTALLER_ISO_DIR)

# --- Stage 3: Build the Installer Kernel ---
$(INSTALLER_ELF): $(INSTALLER_C_OBJECTS) $(INSTALLER_ASM_OBJECTS)
	@echo -e "$(YELLOW)Linking Installer Kernel...$(RESET)"
	$(LD) -m elf_i386 -T linker.ld -o $@ $^

# --- Stage 2: Create the Bootable HDD Image for the Final System ---
$(FINAL_OS_HDD_IMG): $(FINAL_OS_ELF) grub/grub.cfg | $(MNT_DIR)
	@echo -e "$(YELLOW)Creating Final OS Bootable HDD Image (requires sudo)...$(RESET)"
	dd if=/dev/zero of=$@ bs=1M count=15
	sudo sh -c ' \
		set -e; \
		LOOP_DEV=$$(losetup --show --find $@); \
		echo "Using loop device: $$LOOP_DEV"; \
		parted -s $$LOOP_DEV mklabel msdos; \
		parted -s $$LOOP_DEV mkpart primary fat32 2048s 100%; \
		parted -s $$LOOP_DEV set 1 boot on; \
		partprobe $$LOOP_DEV; \
		mkfs.fat -F 32 $${LOOP_DEV}p1; \
		mount $${LOOP_DEV}p1 $(MNT_DIR); \
		echo "Installing GRUB to MBR of $$LOOP_DEV"; \
		# *** THE CORE FIX IS HERE: Add --modules to pre-load drivers *** \
		grub-install --target=i386-pc --boot-directory=$(MNT_DIR)/boot --no-floppy \
			--modules="normal part_msdos fat" $$LOOP_DEV; \
		echo "Copying kernel and grub config..."; \
		cp $(FINAL_OS_ELF) $(MNT_DIR)/boot/kernel; \
		cp grub/grub.cfg $(MNT_DIR)/boot/grub/grub.cfg; \
		echo "Cleaning up mount point and loop device..."; \
		umount $(MNT_DIR); \
		losetup -d $$LOOP_DEV; \
	'

# --- Stage 1: Build the Final OS Kernel ---
$(FINAL_OS_ELF): $(OS_C_OBJECTS) $(OS_ASM_OBJECTS) | $(BIN_DIR)
	@echo -e "$(YELLOW)Linking Final OS Kernel...$(RESET)"
	$(LD) -m elf_i386 -T linker.ld -o $@ $^

# ==============================================================================
# --- COMPILATION & ASSEMBLY RULES ---
# ==============================================================================

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo -e "$(BLUE)Compiling $< -> $@$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: live/%.c | $(OBJ_DIR)
	@echo -e "$(CYAN)Compiling (Live) $< -> $@$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/boot.o: boot.asm | $(OBJ_DIR)
	@echo -e "$(BLUE)Assembling $< -> $@$(RESET)"
	$(ASM) -f elf32 $< -o $@

$(OBJ_DIR)/os_image.o: live/os_image.asm $(FINAL_OS_HDD_IMG) | $(OBJ_DIR)
	@echo -e "$(CYAN)Assembling (Live) $< -> $@$(RESET)"
	$(ASM) -f elf32 $< -o $@

# ==============================================================================
# --- UTILITY TARGETS ---
# =================================================.PHONY: all clean

all: $(INSTALLER_ISO)
	@echo "$(GREEN)Build finished successfully! Installer ISO is ready: $(INSTALLER_ISO)$(RESET)"

# --- Stage 4: Create the Final Installer ISO ---
$(INSTALLER_ISO): $(INSTALLER_ELF) grub/grub.cfg | $(INSTALLER_ISO_DIR)
	@echo -e "$(YELLOW)Creating Installer ISO image...$(RESET)"
	rm -rf $(INSTALLER_ISO_DIR)/*
	mkdir -p $(INSTALLER_ISO_DIR)/boot/grub
	cp $(INSTALLER_ELF) $(INSTALLER_ISO_DIR)/boot/kernel
	cp grub/grub.cfg $(INSTALLER_ISO_DIR)/boot/grub/
	$(GRUB_MK) -o $@ $(INSTALLER_ISO_DIR)

# --- Stage 3: Build the Installer Kernel ---
$(INSTALLER_ELF): $(INSTALLER_C_OBJECTS) $(INSTALLER_ASM_OBJECTS)
	@echo -e "$(YELLOW)Linking Installer Kernel...$(RESET)"
	$(LD) -m elf_i386 -T linker.ld -o $@ $^

# --- Stage 2: Create the Bootable HDD Image for the Final System ---
# *** THIS IS THE NEW, ROBUST METHOD. IT REQUIRES SUDO. ***
$(FINAL_OS_HDD_IMG): $(FINAL_OS_ELF) grub/grub.cfg | $(MNT_DIR)
	@echo -e "$(YELLOW)Creating Final OS Bootable HDD Image (requires sudo)...$(RESET)"
	# Create a 15MB blank file
	dd if=/dev/zero of=$@ bs=1M count=15
	# Set up a loopback device FIRST, so we can operate on it
	# The 'sh -c' block ensures cleanup happens even if something fails
	sudo sh -c ' \
		set -e; \
		LOOP_DEV=$$(losetup --show --find $@); \
		echo "Using loop device: $$LOOP_DEV"; \
		\
		echo "Creating partition table on $$LOOP_DEV"; \
		parted -s $$LOOP_DEV mklabel msdos; \
		parted -s $$LOOP_DEV mkpart primary fat32 2048s 100%; \
		parted -s $$LOOP_DEV set 1 boot on; \
		\
		echo "Forcing kernel to re-read partition table..."; \
		partprobe $$LOOP_DEV; \
		\
		echo "Formatting partition $${LOOP_DEV}p1..."; \
		mkfs.fat -F 32 $${LOOP_DEV}p1; \
		\
		mount $${LOOP_DEV}p1 $(MNT_DIR); \
		echo "Installing GRUB to MBR of $$LOOP_DEV"; \
		# *** THE CORE FIX IS HERE: Add --modules to pre-load drivers *** \
		grub-install --target=i386-pc --boot-directory=$(MNT_DIR)/boot --no-floppy \
			--modules="normal part_msdos fat" $$LOOP_DEV; \
		\
		echo "Copying kernel and grub config..."; \
		cp $(FINAL_OS_ELF) $(MNT_DIR)/boot/kernel; \
		cp grub/grub.cfg $(MNT_DIR)/boot/grub/grub.cfg; \
		\
		echo "Cleaning up mount point and loop device..."; \
		umount $(MNT_DIR); \
		losetup -d $$LOOP_DEV; \
	'

# --- Stage 1: Build the Final OS Kernel ---
$(FINAL_OS_ELF): $(OS_C_OBJECTS) $(OS_ASM_OBJECTS) | $(BIN_DIR)
	@echo -e "$(YELLOW)Linking Final OS Kernel...$(RESET)"
	$(LD) -m elf_i386 -T linker.ld -o $@ $^


# ==============================================================================
# --- COMPILATION & ASSEMBLY RULES ---
# ==============================================================================

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo -e "$(BLUE)Compiling $< -> $@$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: live/%.c | $(OBJ_DIR)
	@echo -e "$(CYAN)Compiling (Live) $< -> $@$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/boot.o: boot.asm | $(OBJ_DIR)
	@echo -e "$(BLUE)Assembling $< -> $@$(RESET)"
	$(ASM) -f elf32 $< -o $@

$(OBJ_DIR)/os_image.o: live/os_image.asm $(FINAL_OS_HDD_IMG) | $(OBJ_DIR)
	@echo -e "$(CYAN)Assembling (Live) $< -> $@$(RESET)"
	$(ASM) -f elf32 $< -o $@

# ==============================================================================
# --- UTILITY TARGETS ---
# ==============================================================================

$(OBJ_DIR) $(BIN_DIR) $(MNT_DIR) $(INSTALLER_ISO_DIR):
	mkdir -p $@

clean:
	@echo -e "$(RED)Cleaning all build artifacts... (may require sudo for leftover loop devices)$(RESET)"
	-sudo umount $(MNT_DIR) 2>/dev/null || true
	rm -rf $(BUILD_DIR) $(INSTALLER_ISO)