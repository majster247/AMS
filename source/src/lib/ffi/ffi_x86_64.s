; ffi_call_asm(fn, args, nargs, rval)
;   rdi = fn        — function pointer
;   rsi = args      — uint64_t[nargs] array
;   rdx = nargs     — number of arguments (≤ 6 for reg-only path)
;   rcx = rval      — pointer to return value storage (may be NULL)
;
; Passes up to 6 args in RDI,RSI,RDX,RCX,R8,R9 per SysV ABI.
; Returns integer/pointer result in RAX; stores it at *rval if non-NULL.

global ffi_call_asm
ffi_call_asm:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov  r12, rdi       ; fn
    mov  r13, rsi       ; args array
    mov  r14, rdx       ; nargs
    mov  r15, rcx       ; rval

    ; Load integer arguments into registers
    xor  rdi, rdi
    xor  rsi, rsi
    xor  rdx, rdx
    xor  rcx, rcx
    xor  r8,  r8
    xor  r9,  r9

    cmp  r14, 1
    jl   .do_call
    mov  rdi, [r13 + 0]
    cmp  r14, 2
    jl   .do_call
    mov  rsi, [r13 + 8]
    cmp  r14, 3
    jl   .do_call
    mov  rdx, [r13 + 16]
    cmp  r14, 4
    jl   .do_call
    mov  rcx, [r13 + 24]
    cmp  r14, 5
    jl   .do_call
    mov  r8,  [r13 + 32]
    cmp  r14, 6
    jl   .do_call
    mov  r9,  [r13 + 40]

.do_call:
    xor  al, al          ; no XMM args
    call r12

    ; Store return value
    test r15, r15
    jz   .done
    mov  [r15], rax

.done:
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret
