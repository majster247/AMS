global _start
extern main
extern exit

section .text
_start:
    ; Kernel wrzucił na stos:
    ; [RSP]    = argc (ilość argumentów)
    ; [RSP+8]  = argv[0] (wskaźnik do nazwy programu)
    ; [RSP+16] = argv[1] ...
    
    ; ABI Linuxa / System V oczekuje:
    ; RDI = argc
    ; RSI = argv (wskaźnik na tablicę wskaźników)
    
    mov rdi, [rsp]      ; Weź argc ze szczytu stosu
    lea rsi, [rsp+8]    ; argv to adres zaraz za argc (czyli RSP + 8)
    
    ; Wyrównanie stosu (opcjonalne, ale dobre dla SSE)
    ; and rsp, -16
    
    call main           ; main(argc, argv)
    
    ; Jeśli main wróci, wywołaj exit z kodem powrotu (w RAX)
    mov rdi, rax
    call exit
    
    ; Bezpiecznik (gdyby exit nie zadziałał)
    mov rax, 60
    syscall