[BITS 64]

; --- MAKRA BEZ ZMIAN ---
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli
        push 0                  ; Dummy error code
        push %1                 ; Numer ISR
        jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli
        push %1                 ; Numer ISR (kod błędu już jest)
        jmp isr_common_stub
%endmacro

%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push 0                  ; Dummy error code
        push %2                 ; Numer IRQ
        jmp irq_common_stub
%endmacro

; --- LISTA PRZERWAŃ (BEZ ZMIAN) ---
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
ISR_NOERRCODE 30
ISR_NOERRCODE 31

ISR_NOERRCODE 128

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

extern isr_handler
extern irq_handler
extern schedule

; ----------------------------------------------------
; STUB DLA WYJĄTKÓW
; ----------------------------------------------------
isr_common_stub:
    ; Zapisujemy tylko 15 rejestrów ogólnych (zgodnie ze struct registers w C++)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Ustawiamy segmenty jądra (ważne, jeśli przerwanie przyszło z Ring 3)
    xor rax, rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rdi, rsp       ; Arg 1: wskaźnik na registers (stos)
    call isr_handler
    jmp common_exit

; ----------------------------------------------------
; STUB DLA SPRZĘTU (IRQ)
; ----------------------------------------------------
irq_common_stub:
    ; 1. Zapisz 15 rejestrów (15 * 8 = 120 bajtów)
    ; UWAGA: NIE zapisujemy tutaj DS na stosie, bo psuje to wyrównanie z C++!
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Ustaw segmenty Kernela
    xor rax, rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; 3. Wywołaj Handler C++
    mov rdi, rsp       ; 1. arg: registers*

    ; POPRAWIONY OFFSET: 120 (15 rejestrów * 8 bajtów)
    ; [rsp + 0]   = r15
    ; ...
    ; [rsp + 112] = rax
    ; [rsp + 120] = Numer Przerwania (to co pushnęło makro)
    mov rsi, [rsp + 120] 
    
    call irq_handler

    ; 4. Scheduler (Dla IRQ0 / 32)
    mov rbx, [rsp + 120] ; Pobierz numer ponownie (offset 120!)
    cmp rbx, 32
    jne common_exit
    
    mov rdi, rsp
    call schedule      ; schedule zwraca nowy RSP (może być ze stosu Usera)
    mov rsp, rax       ; Podmiana stosu

common_exit:
    ; 1. Przywróć 15 rejestrów
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; W tym punkcie RSP wskazuje na int_no.
    ; Układ stosu: [int_no], [err_code], [RIP], [CS], [RFLAGS], [RSP], [SS]

    ; 2. Sprawdź Ring (CS jest 24 bajty od obecnego RSP)
    test byte [rsp + 24], 3
    jz .skip_segment_reset

    ; Powrót do Ring 3 - zerujemy DS/ES (używamy AX, bo RAX już przywrócony!)
    push rax
    xor ax, ax
    mov ds, ax
    mov es, ax
    pop rax

.skip_segment_reset:
    ; 3. Usuwamy int_no i err_code
    add rsp, 16

    ; 4. FINALNY POWRÓT
    iretq

global switch_to_kernel_stack

; void switch_to_kernel_stack(void* new_stack, void (*func)())
; RDI = Nowy Stos
; RSI = Funkcja do skoku
global switch_to_kernel_stack
switch_to_kernel_stack:
    cli                 
    mov rsp, rdi        
    mov rbp, rsp        
    
    and rsp, -16        ; Wyrównaj do 16 bajtów
    ; sub rsp, 8        <-- USUŃ TO. To psuło wyrównanie przy jmp.
    
    jmp rsi

.hang:
    hlt
    jmp .hang