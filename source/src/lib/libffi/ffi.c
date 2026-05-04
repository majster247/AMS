/**
 * @file ffi.c
 * @brief Minimal libffi implementation for AMS (x86_64).
 *
 * Supports up to 6 integer/pointer arguments via the System V ABI
 * register convention (rdi, rsi, rdx, rcx, r8, r9).
 * This is sufficient for Wayland's closure dispatch mechanism.
 */

#include "ffi.h"
#include <stdlib.h>
#include <string.h>

ffi_type ffi_type_void    = { 0, 1, FFI_TYPE_VOID,    NULL };
ffi_type ffi_type_uint8   = { 1, 1, FFI_TYPE_UINT8,   NULL };
ffi_type ffi_type_sint8   = { 1, 1, FFI_TYPE_SINT8,   NULL };
ffi_type ffi_type_uint16  = { 2, 2, FFI_TYPE_UINT16,  NULL };
ffi_type ffi_type_sint16  = { 2, 2, FFI_TYPE_SINT16,  NULL };
ffi_type ffi_type_uint32  = { 4, 4, FFI_TYPE_UINT32,  NULL };
ffi_type ffi_type_sint32  = { 4, 4, FFI_TYPE_SINT32,  NULL };
ffi_type ffi_type_uint64  = { 8, 8, FFI_TYPE_UINT64,  NULL };
ffi_type ffi_type_sint64  = { 8, 8, FFI_TYPE_SINT64,  NULL };
ffi_type ffi_type_float   = { 4, 4, FFI_TYPE_FLOAT,   NULL };
ffi_type ffi_type_double  = { 8, 8, FFI_TYPE_DOUBLE,  NULL };
ffi_type ffi_type_pointer = { 8, 8, FFI_TYPE_POINTER, NULL };

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi,
                         unsigned int nargs,
                         ffi_type *rtype,
                         ffi_type **atypes) {
    if (!cif) return FFI_BAD_TYPEDEF;
    cif->abi = abi;
    cif->nargs = nargs;
    cif->rtype = rtype;
    cif->arg_types = atypes;
    cif->bytes = 0;
    cif->flags = 0;
    return FFI_OK;
}

typedef uint64_t (*fn0_t)(void);
typedef uint64_t (*fn1_t)(uint64_t);
typedef uint64_t (*fn2_t)(uint64_t, uint64_t);
typedef uint64_t (*fn3_t)(uint64_t, uint64_t, uint64_t);
typedef uint64_t (*fn4_t)(uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*fn5_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef uint64_t (*fn6_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void ffi_call(ffi_cif *cif, void (*fn)(void),
              void *rvalue, void **avalue) {
    if (!cif || !fn) return;
    uint64_t args[6] = {0};
    unsigned n = cif->nargs > 6 ? 6 : cif->nargs;
    for (unsigned i = 0; i < n; i++) {
        if (avalue && avalue[i]) {
            args[i] = *(uint64_t*)avalue[i];
        }
    }
    uint64_t ret;
    switch (n) {
        case 0: ret = ((fn0_t)fn)(); break;
        case 1: ret = ((fn1_t)fn)(args[0]); break;
        case 2: ret = ((fn2_t)fn)(args[0], args[1]); break;
        case 3: ret = ((fn3_t)fn)(args[0], args[1], args[2]); break;
        case 4: ret = ((fn4_t)fn)(args[0], args[1], args[2], args[3]); break;
        case 5: ret = ((fn5_t)fn)(args[0], args[1], args[2], args[3], args[4]); break;
        default:ret = ((fn6_t)fn)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
    }
    if (rvalue && cif->rtype && cif->rtype->type != FFI_TYPE_VOID) {
        *(uint64_t*)rvalue = ret;
    }
}

ffi_closure *ffi_closure_alloc(size_t size, void **code) {
    ffi_closure *c = (ffi_closure*)malloc(size > sizeof(ffi_closure) ? size : sizeof(ffi_closure));
    if (c && code) *code = c;
    return c;
}

void ffi_closure_free(ffi_closure *closure) {
    if (closure) free(closure);
}

ffi_status ffi_prep_closure_loc(ffi_closure *closure,
                                 ffi_cif *cif,
                                 ffi_closure_fun fun,
                                 void *user_data,
                                 void *codeloc) {
    if (!closure || !cif || !fun) return FFI_BAD_TYPEDEF;
    closure->cif = cif;
    closure->fun = fun;
    closure->user_data = user_data;
    (void)codeloc;
    return FFI_OK;
}
