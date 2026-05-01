[bits 32]
section .multiboot
align 8
mb2_header_start:
    dd 0xE85250D6
    dd 0
    dd mb2_header_end - mb2_header_start
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))
    align 8
    dw 5, 0
    dd 20   
    dd 1280, 720, 32
    align 8
    dw 0, 0
    dd 8
mb2_header_end:

section .text
global _start
extern kmain
extern gdt_init

_start:
    cli
    mov esp, stack_top
    mov edi, ebx            ; Multiboot pointer do EDI

    ; Czyszczenie tablic stron
    mov edx, p4_table
    mov eax, edx
    mov ecx, 1024 * 3
    .clear_paging:
        mov dword [eax], 0
        add eax, 4
        loop .clear_paging

    ; Setup paging (Identity mapping pierwszych 2MB)
    mov eax, p3_table
    or eax, 0b11
    mov [p4_table], eax

    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax

    mov ecx, 0
.map_p2:
    mov eax, 0x200000    ; 2MB
    mul ecx              ; eax = ecx * 2MB
    or eax, 0b10000011   ; Huge page (bit 7) + Writable + Present
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2

    ; Włącz PAE i Long Mode
    mov eax, p4_table
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_pointer]
    jmp 0x08:start64

[bits 64]
start64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; --- KLUCZOWA POPRAWKA ---
    push rdi                ; Zachowaj adres Multiboot przed wywołaniem GDT
    call gdt_init           ; Inicjalizacja GDT (może nadpisać RDI)
    pop rdi                 ; Przywróć adres Multiboot do RDI dla kmain
    ; -------------------------

    call kmain
    cli
    hlt

section .data
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.data: equ $ - gdt64
    dq (1<<41) | (1<<44) | (1<<47)
gdt64_pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
global stack_bottom    ; Musi być dokładnie taka nazwa
global stack_top
p4_table: resb 4096
p3_table: resb 4096
p2_table: resb 4096
stack_bottom:
    resb 32768
stack_top: