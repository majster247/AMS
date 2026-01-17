ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(8) : { KEEP(*(.multiboot)) }

    .text ALIGN(8) : { *(.text) }

    .rodata ALIGN(8) : { *(.rodata) }

    .bss ALIGN(8) : { *(.bss) *(COMMON) }
}
