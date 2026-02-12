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