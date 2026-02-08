ENTRY(_start)
SECTIONS {
    . = 0x400000; /* Standardowy adres dla ELF */
    .text : { *(.text) }
    .data : { *(.data) }
    .bss  : { *(.bss) }
}