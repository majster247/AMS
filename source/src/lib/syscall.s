[bits 64]
global ams_syscall

section .text
; ams_syscall(rdi:num, rsi:p1, rdx:p2, rcx:p3, r8:p4, r9:p5)
ams_syscall:
    mov rax, rdi    ; ID do RAX
    mov rdi, rsi    ; p1
    mov rsi, rdx    ; p2
    mov rdx, rcx    ; p3
    mov r10, r8     ; p4 (TU JEST FIX: Linux używa R10, bo syscall niszczy RCX)
    mov r8, r9      ; p5
    syscall         ; Skok do jądra
    ret