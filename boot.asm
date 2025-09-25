; boot.asm – Multiboot header and entry point (32-bit protected mode)
bits 32

section .multiboot           ; Multiboot header (GRUB reads this)
    dd 0x1BADB002           ; Magic number
    dd 0x0                  ; Flags (none)
    dd - (0x1BADB002 + 0x0) ; Checksum (so all three sum to 0)

section .text
    global start
    extern main             ; The C kernel function

start:
    cli                     ; Disable interrupts
    mov esp, stack_space    ; Set stack pointer
    call main               ; Call C kernel entry (kernel.c)
    hlt                     ; Halt when done

section .bss
    resb 8192               ; Reserve 8 KB for stack
stack_space: