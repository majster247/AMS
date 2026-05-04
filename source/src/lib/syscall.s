[bits 64]
global ams_syscall
global ams_syscall6

section .text
; ams_syscall(rdi:num, rsi:p1, rdx:p2, rcx:p3, r8:p4, r9:p5)
ams_syscall:
    mov rax, rdi    ; ID do RAX
    mov rdi, rsi    ; p1
    mov rsi, rdx    ; p2
    mov rdx, rcx    ; p3
    mov r10, r8     ; p4 (Linux uses R10 because syscall clobbers RCX)
    mov r8, r9      ; p5
    syscall
    ret

; ams_syscall6(rdi:num, rsi:p1, rdx:p2, rcx:p3, r8:p4, r9:p5, [rsp+8]:p6)
ams_syscall6:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8, r9
    mov r9, [rsp+8]  ; 7th C arg is first stack arg
    syscall
    ret