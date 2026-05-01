; syscall_entry.s
[bits 64]
global syscall_entry
extern syscall_handler
extern cpu_data

section .text
syscall_entry:
    ; Nie polegamy na stanie SWAPGS między taskami.
    mov [rel syscall_user_rsp_scratch], rsp
    mov rsp, [rel cpu_data + 8]  ; CpuData::kernel_stack
    
    ; Budujemy ramkę IRETQ (identyczna z ramką przerwania)
    push 0x2B               ; SS (User Data)
    push qword [rel syscall_user_rsp_scratch] ; RSP (User Stack)
    push r11                ; RFLAGS (zapisane przez procesor w r11 podczas syscall)
    push 0x33               ; CS (User Code)
    push rcx                ; RIP (adres powrotu zapisany przez procesor w rcx)
    
    push 0                  ; err_code
    push rax                ; int_no (w Linux ABI numer syscalla jest w RAX)

    ; Budujemy struct registers dokładnie jak w include/task.h:
    ; r15,r14,r13,r12,r11,r10,r9,r8,rbp,rdi,rsi,rdx,rcx,rbx,rax
    ; Żeby RSP wskazywał na r15, push musi iść w kolejności od rax do r15.
    push rax                ; RAX (numer syscalla / wartość zwrotna)
    push rbx
    push rcx                ; Oryginalne RCX użytkownika
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp            ; RDI = wskaźnik na struct registers
    
    ; ABI x86_64 wymaga wyrównania stosu do 16 bajtów przed call
    ; Mamy pchnięte: 5 (iret) + 2 (info) + 15 (regs) = 22 qwords. 
    ; 22 * 8 = 176 bajtów. 176 / 16 = 11.0 (jest wyrównane)
    
    call syscall_handler

    ; Po powrocie z handlera RAX zawiera wynik syscalla.
    ; Odtwarzamy resztę rejestrów i pomijamy zapisane RAX, żeby nie nadpisać wyniku.
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    add rsp, 8              ; Pomiń zapisane RAX z ramki registers

    add rsp, 16             ; Przeskocz int_no i err_code
    
    iretq                   ; Skok do Ring 3

global switch_to_kernel_stack
switch_to_kernel_stack:
    ; RDI = nowy RSP (kstack_top)
    ; RSI = adres funkcji (kmain_post_stack_switch)

    test rdi, rdi
    jz switch_fatal        ; Zmieniono z .fatal na switch_fatal
    test rsi, rsi
    jz switch_fatal

    ; Przełączamy stos
    mov rsp, rdi
    
    ; Wyrównanie stosu do 16 bajtów (ABI)
    and rsp, -16
    sub rsp, 8             ; Sztuczny powrót (alignment)

    xor rbp, rbp           ; Czyścimy frame pointer
    
    ; Skok przez push/ret (najbezpieczniejszy w 64-bit)
    push rsi
    ret

switch_fatal:              ; Etykieta bez kropki - teraz jest bezpieczna
    cli
.halt_loop:
    hlt
    jmp .halt_loop

section .bss
align 8
syscall_user_rsp_scratch: resq 1