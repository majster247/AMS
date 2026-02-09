[BITS 64]

; Eksportujemy symbole
global jump_to_ring3
global syscall_entry
global force_stack_switch

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

    push 0x1B       ; SS (User Data)
    push rsi        ; RSP (User Stack)
    push 0x202      ; RFLAGS (Interrupts ON)
    push 0x23       ; CS (User Code Selector 0x20 | 3)
    push rdi        ; RIP (Program Entry)
    
    iretq           ; Skok do Ring 3!

; ---------------------------------------------------------
; Entry Point dla instrukcji SYSCALL
; ---------------------------------------------------------
syscall_entry:
    swapgs          ; Zamień User GS na Kernel GS ( CpuData )

    ; Zapisujemy stos użytkownika w strukturze CpuData (offset 0)
    mov [gs:0], rsp 
    ; Ładujemy bezpieczny stos jądra (offset 8)
    mov rsp, [gs:8]

    ; Budujemy strukturę 'registers' na stosie, aby pasowała do handlera C++
    ; Musimy zachować kolejność zgodną z Twoją definicją struct registers
    push 0x1B           ; ss
    push qword [gs:0]   ; rsp (stary stos)
    push r11            ; rflags (syscall kopiuje rflags do r11)
    push 0x23           ; cs
    push rcx            ; rip (syscall kopiuje rip do rcx)
    
    push rax            ; error_code (dummy)
    push 0x0            ; int_no (dummy)

    ; Popychamy rejestry ogólnego przeznaczenia (r15 - r8, rbp - rax)
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

    ; Teraz RSP wskazuje na początek struktury 'registers'
    mov rdi, rsp        ; Przekaż wskaźnik na registers jako pierwszy argument
    
    call syscall_handler

    ; Po powrocie z handlera odtwarzamy stan
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

    ; Pomijamy int_no i error_code
    add rsp, 16

    ; Przygotowujemy powrót dla SYSRET
    pop rcx             ; Odtwórz RIP do RCX
    add rsp, 8          ; Pomiń CS
    pop r11             ; Odtwórz RFLAGS do R11
    pop rsp             ; Odtwórz User RSP

    swapgs              ; Przywróć User GS
    sysretq             ; Powrót do Ring 3!

; ---------------------------------------------------------
; Funkcja przełączania stosu (używana przy starcie GUI)
; ---------------------------------------------------------
force_stack_switch:
    mov rsp, rdi    ; Ładujemy nowy stos
    mov rbp, rsp    
    sub rsp, 8      ; Wyrównanie dla ABI
    call rsi        
    .hang:
        hlt
        jmp .hang