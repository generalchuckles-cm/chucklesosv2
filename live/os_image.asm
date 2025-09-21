; live/os_image.asm - Embeds the final, bootable OS HDD Image
bits 32

section .rodata
    global os_image_start
    global os_image_end

os_image_start:
    ; *** MODIFIED: Embed the entire final HDD Image file ***
    ; The Makefile will create this file before assembling this one.
    incbin "build/chucklesos_final.img"
os_image_end:
    ; This label marks the end of the embedded data.