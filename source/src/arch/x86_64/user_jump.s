[BITS 64]

; Eksportujemy symbole
global jump_to_ring3
global syscall_entry

; Importujemy handler C++
extern syscall_handler

; ---------------------------------------------------------
; Funkcja skoku do Ring 3
; RDI = Adres kodu (RIP)
; RSI = Adres stosu (RSP)
; ---------------------------------------------------------
jump_to_ring3:
    mov ax, 0x1B    ; User Data Selector (0x18 | 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax      ; User GS

    push 0x1B       ; SS
    push rsi        ; RSP
    push 0x202      ; RFLAGS (Interrupts ON)
    push 0x23       ; CS (User Code Selector 0x20 | 3)
    push rdi        ; RIP
    
    iretq           ; Skok!

; ---------------------------------------------------------
; Entry Point dla instrukcji SYSCALL
; Tutaj procesor skacze z Ring 3 -> Ring 0 bez zmiany stosu!
; ---------------------------------------------------------
syscall_entry:
    ; 1. Zamień GS (User GS <-> Kernel GS)
    ; Teraz GS wskazuje na naszą strukturę CpuData (ustawioną w gdt.cpp)
    swapgs

    ; 2. Zapisz stos użytkownika w GS:0 (slot 'self' użyjemy tymczasowo jako scratch)
    ;    lub po prostu w R12/R13 jeśli chcemy być szybcy, ale tu zrobimy bezpiecznie.
    mov [gs:0], rsp 

    ; 3. Załaduj stos jądra z GS:8 (offset kernel_stack w CpuData)
    mov rsp, [gs:8]

    ; 4. Przygotuj stos dla C++ (zgodnie z ABI)
    push 0x1B       ; Old SS (User Data)
    push qword [gs:0] ; Old RSP (User Stack)
    push r11        ; Old RFLAGS (Syscall zapisuje RFLAGS w R11)
    push 0x23       ; Old CS (User Code)
    push rcx        ; Old RIP (Syscall zapisuje RIP powrotu w RCX)

    ; 5. Wywołaj handler w C++
    ; Argumenty w System V ABI: RDI, RSI, RDX, RCX, R8, R9
    ; Nasz syscall: mov $1, rax; mov $msg, rdi
    ; Przekażemy RAX (numer syscalla) jako pierwszy argument (RDI)
    ; A RDI (tekst) jako drugi argument (RSI)
    
    mov rsi, rdi    ; Arg2 = Tekst (był w RDI)
    mov rdi, rax    ; Arg1 = Numer syscalla (był w RAX)
    
    call syscall_handler

    ; 6. Powrót do Ring 3
    pop rcx         ; Przywróć RIP (do RCX dla sysret)
    add rsp, 8      ; Pomiń CS (sysret ustawia go sam)
    pop r11         ; Przywróć RFLAGS (do R11 dla sysret)
    pop rsp         ; Przywróć User RSP
    ; SS jest ignorowany przez sysret

    swapgs          ; Przywróć User GS
    sysretq