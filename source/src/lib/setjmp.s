[BITS 64]
global setjmp
global longjmp

section .text

; int setjmp(jmp_buf env);
; Zapisuje stan rejestrów do bufora
setjmp:
    mov [rdi],    rbx
    mov [rdi+8],  rbp
    mov [rdi+16], r12
    mov [rdi+24], r13
    mov [rdi+32], r14
    mov [rdi+40], r15
    lea rdx, [rsp+8] ; Zapisz stary SP (przed call)
    mov [rdi+48], rdx
    mov rdx, [rsp]   ; Zapisz adres powrotu (RIP)
    mov [rdi+56], rdx
    xor rax, rax     ; setjmp zwraca 0 przy pierwszym wywołaniu
    ret

; void longjmp(jmp_buf env, int val);
; Przywraca stan rejestrów
longjmp:
    mov rax, rsi     ; Wynik (val)
    test rax, rax
    jnz .ok
    inc rax          ; longjmp nie może zwrócić 0
.ok:
    mov rbx, [rdi]
    mov rbp, [rdi+8]
    mov r12, [rdi+16]
    mov r13, [rdi+24]
    mov r14, [rdi+32]
    mov r15, [rdi+40]
    mov rsp, [rdi+48] ; Przywróć stos
    jmp qword [rdi+56]; Skocz do adresu powrotu