; src/lib/crt0.s
[BITS 64]

global _start
extern main
extern exit

section .text

_start:
    ; Tutaj moglibyśmy zdjąć argumenty (argc, argv) ze stosu,
    ; ale na razie zróbmy prosto:
    
    xor rbp, rbp    ; Wyzeruj RBP (oznacza koniec stack trace)
    call main       ; Wywołaj funkcję main() z programu użytkownika
    
    ; Jeśli main wróci (return), wynik jest w RAX
    mov rdi, rax    ; Przekaż wynik jako kod wyjścia
    call exit       ; Zawołaj exit() z naszej biblioteki
    
    hlt             ; Tu nigdy nie dojdziemy