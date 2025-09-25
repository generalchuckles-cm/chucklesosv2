OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS
{
  /* The program begins at the binary load address */
  . = 0x200000;

  .text :
  {
    *(.text)
  }

  .data :
  {
    *(.data)
  }

  .bss :
  {
    *(.bss)
  }

  /* Discard unnecessary sections to keep the binary small */
  /DISCARD/ :
  {
    *(.comment)
    *(.eh_frame)
  }
}