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
    
    push 0                  ; err_code
    push 123                  ; int_no

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
    call syscall_handler

    ; 4. Powrót
    pop rax
    pop rbx
    pop r10         ; rcx_arg (R10 przekazane jako RCX)
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10         ; oryginalne r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    add rsp, 16     ; Przeskocz int_no i err_code
    
    ; TERAZ RSP wskazuje na RIP (ramka CPU)
    ; Używamy IRETQ zamiast SYSRETQ dla testu - on zdejmie 5 wartości:
    ; RIP, CS, RFLAGS, RSP, SS
    
    swapgs          ; Przywróć GS użytkownika
    iretq           ; Pancerny powrót