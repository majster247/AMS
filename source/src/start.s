global _start

section .multiboot
align 8
mb2_header_start:
    dd 0xE85250D6          ; magic
    dd 0                    ; architecture
    dd mb2_header_end - mb2_header_start  ; header length
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start)) ; checksum
    dw 0                    ; type = 0 (end tag)
    dw 0                    ; flags
    dd 8                    ; size = 8
mb2_header_end:

section .bss
align 16
stack_space: resb 16384

section .text
_start:
    lea rsp, [rel stack_space + 16384]  ; ustawienie stosu
    and rsp, -16                        ; wyrównanie 16B

    extern kmain
    call kmain                           ; wywołanie Twojego kernel.cpp

hang:
    hlt
    jmp hang
