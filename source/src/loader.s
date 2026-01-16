BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7C00

    ; -------- GDT --------
gdt_start:
    dq 0x0000000000000000      ; Null
    dq 0x00AF9A000000FFFF      ; Code64, L=1
    dq 0x00AF92000000FFFF      ; Data64
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start
    lgdt [gdt_descriptor]

    ; Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm32

[BITS 32]
pm32:
    ; Segmenty flat
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; -------- Page Tables --------
    ; 4KB alignment: PML4, PDPT, PD
    ; Identity map 0x0 – 0x200000
    mov eax, 0x00001000
    mov [pml4], eax
    mov eax, 0x00002000
    mov [pdpt], eax
    mov eax, 0x00003003
    mov [pd], eax        ; RW, Present

    ; -------- Long Mode --------
    mov eax, cr4
    or eax, 1 << 5       ; PAE
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8       ; LME
    wrmsr

    mov eax, cr0
    or eax, 0x80000000   ; PG
    mov cr0, eax

    jmp 0x08:0x100000    ; Skok do kernela 64-bit

times 510-($-$$) db 0
dw 0xAA55

; --------- Page Tables --------
align 4096
pml4:  dq 0
pdpt:  dq 0
pd:    dq 0
