[BITS 64]

; --- MAKRA DO GENEROWANIA ENTRY POINTÓW ---
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli
        push 0                  ; Dummy error code
        push %1                 ; Numer przerwania
        jmp common_stub
%endmacro

%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli
        push %1                 ; Numer przerwania (kod błędu już jest na stosie)
        jmp common_stub
%endmacro

%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push 0                  ; Dummy error code
        push %2                 ; Mapowanie na IDT (np. IRQ0 -> 32)
        jmp common_stub
%endmacro

; --- DEFINICJE PRZERWAŃ ---
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; --- IRQ (Mapowane od 32 wzwyż) ---
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; --- SYSCALL (Legacy INT 0x80) ---
global isr128
isr128:
    cli
    push 0
    push 128
    jmp common_stub

; --- COMMON STUB ---
extern interrupt_handler
extern schedule

common_stub:
    ; Pchanie rejestrów w kolejności struktury 'registers' (od tyłu)
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; Sprawdzenie czy przerwanie przyszło z User Mode (Ring 3)
    ; CS znajduje się pod [rsp + 120 + 16] (15 regs + 2 info)
    test qword [rsp + 136], 3 
    jz .in_kernel
    swapgs                  ; Wejście z User -> załaduj Kernel GS
.in_kernel:

    mov rdi, rsp            ; RDI = registers*
    call interrupt_handler

    ; Obsługa Schedulera (tylko dla Timer IRQ0 = 32)
    cmp qword [rsp + 120], 32
    jne .no_reschedule
    
    mov rdi, rsp
    call schedule
    mov rsp, rax            ; Przełączenie stosu na nowy proces
.no_reschedule:

    ; Wyjście: Jeśli wracamy do Ring 3, swapgs z powrotem
    test qword [rsp + 136], 3
    jz .skip_swapgs
    swapgs
.skip_swapgs:

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    
    add rsp, 16             ; Usuń int_no i err_code
    iretq