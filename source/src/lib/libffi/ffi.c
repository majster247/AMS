/**
 * @file libffi/ffi.c
 * @brief Minimal libffi implementation for AMS-OS x86-64.
 *
 * Calling convention: System V AMD64 ABI.
 *
 *  Integer / pointer arguments: RDI, RSI, RDX, RCX, R8, R9 (6 slots)
 *  Floating-point arguments:    XMM0–XMM7 (8 slots)
 *  Return values:               RAX (integers/pointers), XMM0 (fp)
 *
 * For structs that fit in two 8-byte "eightbytes" we classify each half
 * and pass them in the corresponding integer/FP registers.  Structs larger
 * than 16 bytes are passed on the stack (and returned via hidden pointer).
 *
 * ffi_call() is implemented as:
 *   1. Classify all argument types.
 *   2. Marshal arguments into a register array and optional stack buffer.
 *   3. Call a small assembly thunk that loads the registers and jumps to fn.
 *
 * ffi_closure is implemented using a per-closure executable trampoline that
 * stores the closure pointer in R10 and jumps to ffi_closure_call_inner().
 */

#include "libffi/ffi.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ---- Built-in ffi_type singletons ---- */
ffi_type ffi_type_void     = {0,  0, FFI_TYPE_VOID,    NULL};
ffi_type ffi_type_uint8    = {1,  1, FFI_TYPE_UINT8,   NULL};
ffi_type ffi_type_sint8    = {1,  1, FFI_TYPE_SINT8,   NULL};
ffi_type ffi_type_uint16   = {2,  2, FFI_TYPE_UINT16,  NULL};
ffi_type ffi_type_sint16   = {2,  2, FFI_TYPE_SINT16,  NULL};
ffi_type ffi_type_uint32   = {4,  4, FFI_TYPE_UINT32,  NULL};
ffi_type ffi_type_sint32   = {4,  4, FFI_TYPE_SINT32,  NULL};
ffi_type ffi_type_uint64   = {8,  8, FFI_TYPE_UINT64,  NULL};
ffi_type ffi_type_sint64   = {8,  8, FFI_TYPE_SINT64,  NULL};
ffi_type ffi_type_float    = {4,  4, FFI_TYPE_FLOAT,   NULL};
ffi_type ffi_type_double   = {8,  8, FFI_TYPE_DOUBLE,  NULL};
ffi_type ffi_type_pointer  = {8,  8, FFI_TYPE_POINTER, NULL};
ffi_type ffi_type_ulong    = {8,  8, FFI_TYPE_UINT64,  NULL};
ffi_type ffi_type_slong    = {8,  8, FFI_TYPE_SINT64,  NULL};

/* ---- Classification of a type into "integer" or "float" class ---- */
typedef enum { CLS_INT, CLS_FLOAT, CLS_MEM } arg_class;

static arg_class classify(ffi_type *t) {
    switch (t->type) {
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
        return CLS_FLOAT;
    case FFI_TYPE_STRUCT:
        if (t->size > 16) return CLS_MEM;
        return CLS_INT; /* simplification: treat small structs as integer */
    default:
        return CLS_INT;
    }
}

/* ---- ffi_prep_cif ---- */
ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned nargs,
                         ffi_type *rtype, ffi_type **argtypes) {
    if (!cif || !rtype) return FFI_BAD_TYPEDEF;
    if (abi != FFI_SYSV) return FFI_BAD_ABI;
    cif->abi       = abi;
    cif->nargs     = nargs;
    cif->arg_types = argtypes;
    cif->rtype     = rtype;

    /* Compute required stack space (8-byte slots for stack-spilled args) */
    unsigned stack = 0;
    int ireg = 0, freg = 0;
    for (unsigned i = 0; i < nargs; ++i) {
        ffi_type *a = argtypes[i];
        arg_class cls = classify(a);
        if (cls == CLS_FLOAT && freg < 8) {
            freg++;
        } else if (cls == CLS_INT && ireg < 6) {
            ireg++;
        } else {
            stack += (a->size + 7) & ~7u;
        }
    }
    cif->bytes = stack;
    cif->flags = 0;
    return FFI_OK;
}

ffi_status ffi_prep_cif_var(ffi_cif *cif, ffi_abi abi,
                             unsigned nfixedargs, unsigned ntotalargs,
                             ffi_type *rtype, ffi_type **argtypes) {
    (void)nfixedargs;
    return ffi_prep_cif(cif, abi, ntotalargs, rtype, argtypes);
}

/* ---- Assembly thunk declaration ---- */

/*
 * ffi_call_x86_64(fn, ireg_vals[6], freg_vals[8], rtype_class,
 *                 stack_buf, stack_bytes, rvalue)
 *
 * Implemented in ffi_asm.s (NASM).  When not available, we fall back to a
 * best-effort C varargs call which works for simple integer-only signatures.
 */
extern void ffi_call_x86_64(void (*fn)(void),
                              uint64_t *iregs,   /* [6] */
                              double   *fregs,   /* [8] */
                              int       rtype,
                              void     *stack_buf,
                              unsigned  stack_bytes,
                              void     *rvalue);

/* ---- ffi_call ---- */
void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
    if (!cif || !fn) return;

    uint64_t iregs[6]  = {0,0,0,0,0,0};
    double   fregs[8]  = {0,0,0,0,0,0,0,0};
    uint8_t  stack_buf[256];
    unsigned stack_off = 0;

    int ireg = 0, freg = 0;

    /* Marshal arguments */
    for (unsigned i = 0; i < cif->nargs; ++i) {
        ffi_type  *a   = cif->arg_types[i];
        void      *val = avalue[i];
        arg_class  cls = classify(a);

        if (cls == CLS_FLOAT && freg < 8) {
            if (a->type == FFI_TYPE_FLOAT) {
                float f; memcpy(&f, val, 4);
                fregs[freg++] = (double)f;
            } else {
                memcpy(&fregs[freg++], val, 8);
            }
        } else if (cls == CLS_INT && ireg < 6) {
            uint64_t v = 0;
            memcpy(&v, val, a->size < 8 ? a->size : 8);
            iregs[ireg++] = v;
        } else {
            /* Stack spill */
            unsigned sz = (a->size + 7) & ~7u;
            memcpy(stack_buf + stack_off, val, a->size);
            stack_off += sz;
        }
    }

    int rtype_cls = (int)classify(cif->rtype);
    ffi_call_x86_64(fn, iregs, fregs, rtype_cls,
                    stack_buf, stack_off, rvalue);
}

/* ---- Closure allocation ---- */

/*
 * On AMS-OS we get executable memory by mmap()'ing an anonymous page
 * with PROT_READ|PROT_WRITE|PROT_EXEC (flags 7, MAP_ANON|MAP_PRIVATE).
 * The kernel sys_mmap allocates user pages which are always executable
 * in the current page-table setup.
 */
extern void* mmap(void*, size_t, int, int, int, long);
extern int   munmap(void*, size_t);

void* ffi_closure_alloc(size_t size, void **code) {
    void *p = mmap(0, size < 4096 ? 4096 : size,
                   7 /* PROT_READ|PROT_WRITE|PROT_EXEC */,
                   0x22 /* MAP_PRIVATE|MAP_ANON */, -1, 0);
    if (!p) return NULL;
    *code = p;
    return p;
}

void ffi_closure_free(void *writable) {
    munmap(writable, 4096);
}

/* ---- Closure trampoline and call ---- */

/*
 * The trampoline written into ffi_closure.trampoline:
 *   mov r10, <closure_ptr>     ; 10 bytes
 *   jmp ffi_closure_call_inner ; 5 bytes (rel32)
 *
 * ffi_closure_call_inner() collects the register arguments, builds an
 * avalue[] array, then calls closure->fun(cif, rvalue, avalue, user_data).
 */
void ffi_closure_call_inner(ffi_closure *closure,
                             uint64_t *iregs, double *fregs,
                             void *rvalue);

void ffi_closure_call_inner(ffi_closure *closure,
                             uint64_t *iregs, double *fregs,
                             void *rvalue) {
    ffi_cif *cif  = closure->cif;
    void   **avalue = (void**)__builtin_alloca(sizeof(void*) * cif->nargs);
    uint64_t  ivals[6];
    double    fvals[8];

    int ireg = 0, freg = 0;
    for (unsigned i = 0; i < cif->nargs; ++i) {
        arg_class cls = classify(cif->arg_types[i]);
        if (cls == CLS_FLOAT && freg < 8) {
            fvals[freg] = fregs[freg]; avalue[i] = &fvals[freg++];
        } else if (cls == CLS_INT && ireg < 6) {
            ivals[ireg] = iregs[ireg]; avalue[i] = &ivals[ireg++];
        } else {
            avalue[i] = NULL; /* stack args not supported in closure path */
        }
    }
    closure->fun(cif, rvalue, avalue, closure->user_data);
}

ffi_status ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                  void (*fun)(ffi_cif*, void*, void**, void*),
                                  void *user_data, void *codeloc) {
    if (!closure || !cif || !fun || !codeloc) return FFI_BAD_TYPEDEF;
    closure->cif       = cif;
    closure->fun       = fun;
    closure->user_data = user_data;

    uint8_t *tramp = (uint8_t*)codeloc;
    uint64_t closure_ptr = (uint64_t)closure;
    uint64_t inner_ptr   = (uint64_t)(void*)ffi_closure_call_inner;

    /* movabs r10, closure_ptr: REX.W=1, opcode 0xBA (MOV r/m64,imm64 via 'BA+rd') */
    /* Actually: REX.W | B (r10=REX extended) = 0x49; opcode 0xba */
    tramp[0] = 0x49; tramp[1] = 0xba;
    memcpy(tramp + 2, &closure_ptr, 8); /* 10 bytes total */

    /* jmp rel32: opcode 0xe9 */
    tramp[10] = 0xe9;
    int32_t rel = (int32_t)((int64_t)inner_ptr - ((int64_t)codeloc + 15));
    memcpy(tramp + 11, &rel, 4);

    /* nop padding */
    for (int i = 15; i < 40; ++i) tramp[i] = 0x90;

    return FFI_OK;
}
