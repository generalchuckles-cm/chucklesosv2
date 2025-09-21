# chucklesosv2
V2 Beta 1 is out!
You can now install it to hard disk!
You can also run binaries, notice, it treats them independently, therefore they must be freestanding and stage their own operations. When there is no more code to run, the OS automaticlly returns to shell. You should use ret though.

Programming for ChucklesOS (Assembly)

Follow these rules to create working programs for ChucklesOS.
The Rules

    Compile to a flat binary: Use nasm -f bin your_program.asm -o your_program.bin.

    No BIOS Interrupts: Do not use int 0x10 or other interrupts. They will crash the OS.

    No Kernel API: Write directly to hardware, like the VGA memory buffer at 0xB8000.

    Code must be Position-Independent: Use the call/pop trick in the template to find your data.

    End with ret: Your program must finish with a ret instruction to return to the shell.

Template Program (template.asm)

Use this as the starting point for your programs. It clears the screen and prints a message.
bits 32

section .text
    global _start

_start:
    ; Clear screen (80x25)
    mov ecx, 2000
    mov edi, 0xB8000
    mov ax, 0x0F20      ; White on Black space character
    rep stosw

    ; Get message address
    call get_address
get_address:
    pop esi
    add esi, msg - get_address

    ; Print message
    mov edi, 0xB8000
    mov ah, 0x0F
print_loop:
    lodsb
    cmp al, 0
    je .done
    mov [edi], ax
    add edi, 2
    jmp print_loop

.done:
    ; Return to shell
    ret

section .data
msg:
    db 'My first program for ChucklesOS!', 0

  

How to Run Your Program

    
nasm -f bin your_program.asm -o your_program.bin

  

Copy to Disk:
Use the diskmnt.py tool to copy your .bin file to the virtual_disk.img.

Execute in ChucklesOS:
Boot the OS, cd to the correct directory, and run it.

    
./your_program.bin

  
