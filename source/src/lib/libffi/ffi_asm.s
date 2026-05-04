; ffi_asm.s – x86-64 System V ABI call thunk for AMS-OS libffi
;
; Prototype (C declaration):
;   void ffi_call_x86_64(void (*fn)(void),
;                         uint64_t *iregs,
;                         double   *fregs,
;                         int       rtype,
;                         void     *stack_buf,
;                         unsigned  stack_bytes,
;                         void     *rvalue);
;
; Arguments (System V AMD64):
;   rdi = fn
;   rsi = iregs  (pointer to uint64_t[6])
;   rdx = fregs  (pointer to double[8])
;   rcx = rtype  (0=int, 1=float)
;   r8  = stack_buf
;   r9  = stack_bytes
;   [rsp+8] = rvalue   <- pushed by caller before call

bits 64
section .text

global ffi_call_x86_64
ffi_call_x86_64:
    ; Prologue – save non-volatile registers
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save arguments into preserved registers
    mov     r12, rdi        ; fn
    mov     r13, rsi        ; iregs
    mov     r14, rdx        ; fregs
    mov     r15, rcx        ; rtype
    ; r8  = stack_buf (already preserved across call by SysV)
    ; r9  = stack_bytes

    ; Push stack arguments (if any) in reverse order
    ; stack_bytes is already a multiple of 8
    test    r9, r9
    jz      .no_stack

    ; align stack: we need (stack_bytes) bytes below rsp, and rsp must be
    ; 16-byte aligned before the call (after the call pushes the return addr).
    ; We subtract stack_bytes, then if misaligned we subtract 8 more.
    sub     rsp, r9
    ; copy stack_buf → rsp
    mov     rdi, rsp
    mov     rsi, r8         ; stack_buf
    mov     rcx, r9
    rep     movsb

.no_stack:
    ; Ensure 16-byte alignment
    and     rsp, ~15

    ; Load FP registers from fregs[]
    movsd   xmm0, [r14 + 0*8]
    movsd   xmm1, [r14 + 1*8]
    movsd   xmm2, [r14 + 2*8]
    movsd   xmm3, [r14 + 3*8]
    movsd   xmm4, [r14 + 4*8]
    movsd   xmm5, [r14 + 5*8]
    movsd   xmm6, [r14 + 6*8]
    movsd   xmm7, [r14 + 7*8]

    ; Load integer registers from iregs[]
    mov     rdi, [r13 + 0*8]
    mov     rsi, [r13 + 1*8]
    mov     rdx, [r13 + 2*8]
    mov     rcx, [r13 + 3*8]
    mov     r8,  [r13 + 4*8]
    mov     r9,  [r13 + 5*8]

    ; al = number of fp regs used (required by vararg functions)
    mov     al, 8

    ; Call the function
    call    r12

    ; Retrieve rvalue from [rbp+16] (the 7th argument, pushed after return addr)
    mov     r13, [rbp + 16]

    ; Store return value
    test    r15, r15        ; rtype == 0 → integer
    jnz     .ret_float

    test    r13, r13
    jz      .done
    mov     [r13], rax
    jmp     .done

.ret_float:
    test    r13, r13
    jz      .done
    movsd   [r13], xmm0

.done:
    ; Restore non-volatile registers and return
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret
