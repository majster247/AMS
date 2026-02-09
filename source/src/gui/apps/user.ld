ENTRY(_start) /* To mówi linkerowi, gdzie zacząć */

SECTIONS
{
    /* Ładujemy aplikacje pod 64MB, żeby nie nadpisać kernela (0-16MB) */
    . = 0x4000000;

    .text : {
        *(.text.prologue)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}