[bits 64]
extern schedule
global timer_handler_stub

timer_handler_stub:
    ; Zgodnie ze strukturą registers w task.h: 
    ; r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax
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

    mov rdi, rsp            ; Przekazujemy wskaźnik na registers*
    call schedule
    mov rsp, rax            ; Przełączamy stos na ten zwrócony przez scheduler

    ; Wyślij EOI do PIC (Inaczej dostaniesz tylko jedno przerwanie!)
    mov al, 0x20
    out 0x20, al

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
    iretq                   ; Powrót z przerwania (zdejmuje RIP, CS, RFLAGS, RSP, SS)