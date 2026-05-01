[BITS 64]

global jump_to_ring3
global jump_to_user
global dbg_ring3_rip
global dbg_ring3_rsp
global dbg_ring3_ksp

section .text

; ---------------------------------------------------------
; void jump_to_ring3(uint64_t rip, uint64_t rsp)
; rdi = RIP (user entry point)
; rsi = RSP (user stack)
; ---------------------------------------------------------
jump_to_ring3:
jump_to_user:
    cli
    cld

    mov [rel dbg_ring3_rip], rdi
    mov [rel dbg_ring3_rsp], rsi
    mov [rel dbg_ring3_ksp], rsp

    ; Linux-like path: don't preload ring3 selectors into data regs in CPL0.
    ; Keep transition state minimal and let IRETQ load CS/SS.
    xor eax, eax
    mov ds, ax
    mov es, ax

    ; ---- frame dla IRETQ (ring3 transition) ----
    push qword 0x2B        ; SS (user data)
    push rsi               ; RSP (user stack)
    push qword 0x202       ; RFLAGS (IF=1, bit1=1, bez flag kernela)
    push qword 0x33        ; CS (user code, RPL3)
    push rdi               ; RIP (entry point)

    iretq

section .bss
align 8
dbg_ring3_rip: resq 1
dbg_ring3_rsp: resq 1
dbg_ring3_ksp: resq 1

