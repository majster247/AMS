ENTRY(kernel_entry)

SECTIONS
{
    . = 0x100000;      /* kernel ładowany pod 0x100000 */
    .text : { *(.text*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
