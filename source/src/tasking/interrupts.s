extern timer_handler_c
extern schedule
global timer_handler_stub

timer_handler_stub:
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

    mov rdi, rsp    
    call timer_handler_c
    call schedule
    mov rsp, rax    ; Tu przeskakujemy na stos innego zadania

    ; EOI (End of Interrupt) dla PIC
    mov al, 0x20
    out 0x20, al

    ; TERAZ POPUJEMY W IDEALNIE ODWROTNEJ KOLEJNOŚCI
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