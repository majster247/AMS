[BITS 64]

; Makra do obsługi wyjątków (z kodem błędu i bez)
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        push 0                  ; Dummy error code
        push %1                 ; Numer przerwania
        jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        push %1                 ; Numer przerwania (kod błędu już jest na stosie)
        jmp isr_common_stub
%endmacro

%macro IRQ 2
    global irq%1
    irq%1:
        push 0                  ; Dummy error code
        push %2                 ; Zmapowany numer (np. 32 dla IRQ0)
        jmp isr_common_stub
%endmacro

; Definicje ISR (0-31)
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

; Definicje IRQ (32-47)
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

; Import handlerów C++
extern isr_handler
extern irq_handler
extern schedule  ; Funkcja schedulera

; -----------------------------------------------
; WSPÓLNY STUB PRZERWAŃ
; -----------------------------------------------
isr_common_stub:
    ; 1. Zapisz wszystkie rejestry (zgodnie ze struct registers w task.h)
    ; Kolejność pushowania musi być odwrotna do pól w strukturze!
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

    ; 2. Zapisz i podmień segmenty danych
    ; To jest KLUCZOWE dla Ring 3! 
    ; Jeśli przyszliśmy z Ring 3, DS i ES mają 0x1B. Jądro potrzebuje 0x10.
    xor rax, rax
    mov ax, ds
    push rax        ; Zapisz stary DS na stosie (jako 64-bit)
    
    mov ax, 0x10    ; Kernel Data Segment
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; 3. Przekaż wskaźnik na stos jako argument (struct registers*)
    mov rdi, rsp

    ; 4. Sprawdź, czy to IRQ (numer > 31) czy Exception
    mov rax, [rsp + 136] ; Pobierz numer przerwania (offset wyliczony: 15 regs * 8 + 1 saved_ds * 8 = 128 + 8 = 136)
    
    cmp rax, 32
    jae .handle_irq

.handle_isr:
    call isr_handler
    jmp .restore

.handle_irq:
    cmp rax, 32
    jne .std_irq
    
    ; IRQ 0 (Timer)
    call irq_handler    ; Teraz irq_handler dostanie numer w RSI
    
    mov rdi, rsp        
    call schedule       
    mov rsp, rax        
    jmp .restore_minimal

.std_irq:
    ; Przekaż numer przerwania jako drugi argument (RSI)
    mov rsi, [rsp + 136] ; Pobierz numer przerwania ze stosu
    mov rdi, rsp         ; Pierwszy argument: struktura registers*
    call irq_handler
    jmp .restore

.restore:
.restore_minimal:
    ; 5. Przywróć segmenty
    pop rax         ; Odtwórz stary DS
    mov ds, ax
    mov es, ax
    ; SS zostanie przywrócony przez iretq (jeśli wracamy do Ring 3)

    ; 6. Przywróć rejestry
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

    ; 7. Usuń kod błędu i numer przerwania ze stosu (2x 8 bajtów)
    add rsp, 16

    ; 8. Powrót
    iretq

global mouse_int_asm_wrapper
extern mouse_handler

mouse_int_asm_wrapper:
    cli
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

    mov rdi, rsp     ; Przekazanie wskaźnika stosu jako argument
    call mouse_handler

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
    
    sti
    iretq
    
