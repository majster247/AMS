[BITS 64]
global jump_to_ring3
global force_stack_switch

jump_to_ring3:
    ; ZMIANA: 0x1B -> 0x2B (User Data 64)
    mov ax, 0x2B    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax      

    ; ZMIANA: Puszujemy nowe selektory
    push 0x2B       ; SS (User Data 64)
    push rsi        ; RSP
    push 0x202      ; RFLAGS
    push 0x33       ; CS (User Code 64)
    push rdi        ; RIP
    
    iretq          

force_stack_switch:
    mov rsp, rdi    
    mov rbp, rsp    
    sub rsp, 8      
    call rsi        
    .hang: hlt
    jmp .hang

global jump_to_user
jump_to_user:
    cli
    
    mov rax, rsi  ; user RSP
    
    ; Pushuj SS (User Data)
    push 0x2B     ; ✅ ZMIENIONE z 0x23 na 0x2B
    
    ; Pushuj user RSP
    push rax
    
    ; Pushuj RFLAGS
    pushfq
    pop r11
    or r11, 0x200
    push r11
    
    ; Pushuj CS (User Code)
    push 0x23     ; ✅ ZMIENIONE z 0x2B na 0x23
    
    ; Pushuj RIP
    push rdi
    
    ; Ustaw segmenty danych
    mov ax, 0x2B  ; ✅ User Data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Wyczyść rejestry
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    iretq