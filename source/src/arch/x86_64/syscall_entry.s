[bits 64]
global syscall_entry
extern syscall_handler

section .text
syscall_entry:
    swapgs
    mov [gs:0x10], rsp
    mov rsp, [gs:0x08]
    
    ; Ramka procesora
    push 0x2B               ; SS
    push qword [gs:0x10]    ; RSP
    push r11                ; RFLAGS
    push 0x33               ; CS
    push rcx                ; RIP (Adres powrotu)
    
    push 0                  ; int_no
    push 0                  ; err_code

    ; Pchamy rejestry... (pamiętaj o kolejności z task.h!)
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
    
    ; --- SYNCHRONIZACJA ARGUMENTU ---
    push r10                ; Wpychamy R10 w miejsce pola RCX struktury!
    ; --------------------------------
    
    push rbx
    push rax

    mov rdi, rsp
    sub rsp, 8              ; Alignment
    call syscall_handler
    add rsp, 8

    ; 4. Powrót
    pop rax                 ; Tu jest wynik BRK (0x4000...)
    pop rbx
    pop rcx                 ; Zdejmij argument (R10)
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

    add rsp, 16             ; Zdejmij err/int

    pop rcx                 ; Odtwórz RIP (zapisany wcześniej adres powrotu)
    add rsp, 8              ; Pomiń CS
    pop r11                 ; RFLAGS
    pop rsp                 ; User RSP
    
    swapgs
    sysretq