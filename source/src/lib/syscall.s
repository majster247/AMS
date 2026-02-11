[BITS 64]

global ams_syscall

; Funkcja: uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3);
; Argumenty wchodzą w (System V ABI): RDI, RSI, RDX, RCX
; Kernel oczekuje (Linux ABI): RAX (num), RDI, RSI, RDX

ams_syscall:
    mov rax, rdi      ; 1. Numer syscalla idzie do RAX
    mov rdi, rsi      ; 2. Parametr 1 idzie do RDI
    mov rsi, rdx      ; 3. Parametr 2 idzie do RSI
    mov rdx, rcx      ; 4. Parametr 3 idzie do RDX
    
    syscall           ; BUM! Skok do kernela (Ring 0)
    
    ret               ; Wynik jest już w RAX