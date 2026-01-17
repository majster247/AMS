; --- start.s ---
[bits 32]

section .multiboot
align 8
mb2_header_start:
    dd 0xE85250D6                           ; Magic (Multiboot 2)
    dd 0                                    ; Architektura i386
    dd mb2_header_end - mb2_header_start    ; Długość nagłówka
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))
    
    ; Tag kończący
    align 8
    dw 0, 0
    dd 8
mb2_header_end:

section .text
global _start
extern kmain

_start:
    cli                                     ; Wyłączenie przerwań
    mov esp, stack_top                      ; Załadowanie bezpiecznego stosu

    ; 1. Czyszczenie tablic stron (PML4, PDPT, PD)
    mov edi, p4_table
    mov ecx, 3072                           ; 3 tablic * 1024 dwordy
    xor eax, eax
    rep stosd

    ; 2. Budowa struktury stronicowania (Identity Mapping pierwszego 1GB)
    ; P4[0] -> P3
    mov eax, p3_table
    or eax, 0b11                            ; Present + Writable
    mov [p4_table], eax

    ; P3[0] -> P2
    mov eax, p2_table
    or eax, 0b11                            ; Present + Writable
    mov [p3_table], eax

    ; P2[0..511] -> 2MB Huge Pages
    mov ecx, 0
.map_p2:
    mov eax, 0x200000                       ; 2MB
    mul ecx
    or eax, 0b10000011                      ; Present + Writable + PS (bit 7)
    mov [p2_table + ecx * 8], eax           ; Zapisz adres (low)
    mov [p2_table + ecx * 8 + 4], edx       ; Zapisz adres (high)
    inc ecx
    cmp ecx, 512
    jne .map_p2

    ; 3. Aktywacja trybu IA-32e (Long Mode)
    mov eax, p4_table
    mov cr3, eax                            ; Załadowanie CR3

    mov eax, cr4
    or eax, (1 << 5) | (1 << 4)              ; PAE + PSE
    mov cr4, eax

    mov ecx, 0xC0000080                      ; EFER MSR
    rdmsr
    or eax, 1 << 8                          ; LME (Long Mode Enable)
    wrmsr

    mov eax, cr0
    or eax, 1 << 31                         ; Paging ON
    mov cr0, eax

    ; 4. Skok do 64-bitowego GDT
    lgdt [gdt64.pointer]
    push 0x08                               ; Selektor kodu (Code Selector)
    push .long_mode
    retf                                    ; Daleki skok do Long Mode

[bits 64]
.long_mode:
    ; Reset rejestrów segmentowych dla Long Mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rdi, rbx                            ; Przekazanie adresu Multiboot Info do kmain
    call kmain                              ; Skok do C++

    ; Pętla bezpieczeństwa
.halt:
    hlt
    jmp .halt

; --- Stubs dla przerwań ---
global isr_keyboard_stub
extern keyboard_handler
isr_keyboard_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    call keyboard_handler                   ;
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

global isr_ignore_stub
isr_ignore_stub:
    push rax
    mov al, 0x20
    out 0x20, al                            ; EOI (End of Interrupt)
    pop rax
    iretq

section .bss
align 4096
stack_bottom:
    resb 32768                              ; 32KB stosu (bezpiecznie poniżej tablic)
stack_top:

align 4096
p4_table: resb 4096                         ; Tablica PML4
p3_table: resb 4096                         ; Tablica PDPT
p2_table: resb 4096                         ; Tablica PD (2MB pages)

section .data
align 16
gdt64:
    dq 0                                    ; Null Descriptor
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53); Code Segment (64-bit, DPL 0)
    dq (1<<44) | (1<<47) | (1<<41)          ; Data Segment (DPL 0)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64                                ; 64-bitowy wskaźnik GDT