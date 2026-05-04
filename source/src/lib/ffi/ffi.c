/* Minimal libffi implementation for AMS — x86_64 System V ABI.
 *
 * We support up to 6 integer/pointer arguments passed in registers
 * (RDI, RSI, RDX, RCX, R8, R9) and an optional return value.
 * Floating-point and struct-by-value arguments are stubbed.
 */
#include "ffi.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- predefined type objects ---- */
ffi_type ffi_type_void    = { 0, 0, FFI_TYPE_VOID,    NULL };
ffi_type ffi_type_pointer = { 8, 8, FFI_TYPE_POINTER, NULL };
ffi_type ffi_type_sint    = { 4, 4, FFI_TYPE_SINT32,  NULL };
ffi_type ffi_type_uint    = { 4, 4, FFI_TYPE_UINT32,  NULL };
ffi_type ffi_type_sint8   = { 1, 1, FFI_TYPE_SINT8,   NULL };
ffi_type ffi_type_uint8   = { 1, 1, FFI_TYPE_UINT8,   NULL };
ffi_type ffi_type_sint16  = { 2, 2, FFI_TYPE_SINT16,  NULL };
ffi_type ffi_type_uint16  = { 2, 2, FFI_TYPE_UINT16,  NULL };
ffi_type ffi_type_sint32  = { 4, 4, FFI_TYPE_SINT32,  NULL };
ffi_type ffi_type_uint32  = { 4, 4, FFI_TYPE_UINT32,  NULL };
ffi_type ffi_type_sint64  = { 8, 8, FFI_TYPE_SINT64,  NULL };
ffi_type ffi_type_uint64  = { 8, 8, FFI_TYPE_UINT64,  NULL };
ffi_type ffi_type_float   = { 4, 4, FFI_TYPE_FLOAT,   NULL };
ffi_type ffi_type_double  = { 8, 8, FFI_TYPE_DOUBLE,  NULL };

ffi_status ffi_prep_cif(ffi_cif* cif, ffi_abi abi, unsigned nargs,
                        ffi_type* rtype, ffi_type** atypes) {
    if (!cif || !rtype) return FFI_BAD_TYPEDEF;
    cif->abi       = abi;
    cif->nargs     = nargs;
    cif->rtype     = rtype;
    cif->arg_types = atypes;
    cif->bytes     = 0;
    cif->flags     = 0;
    return FFI_OK;
}

/* ffi_call is implemented in ffi_x86_64.s */
extern void ffi_call_asm(void (*fn)(void), uint64_t* args, unsigned nargs, void* rval);

void ffi_call(ffi_cif* cif, void (*fn)(void), void* rvalue, void** avalue) {
    /* Build a flat array of uint64_t values from avalue pointers */
    uint64_t args[32];
    unsigned n = cif->nargs < 32 ? cif->nargs : 32;
    for (unsigned i = 0; i < n; i++) {
        if (!avalue || !avalue[i]) { args[i] = 0; continue; }
        ffi_type* t = cif->arg_types ? cif->arg_types[i] : &ffi_type_uint64;
        switch (t->type) {
        case FFI_TYPE_UINT8:   args[i] = *(uint8_t*)avalue[i];  break;
        case FFI_TYPE_SINT8:   args[i] = (uint64_t)(int64_t)*(int8_t*)avalue[i]; break;
        case FFI_TYPE_UINT16:  args[i] = *(uint16_t*)avalue[i]; break;
        case FFI_TYPE_SINT16:  args[i] = (uint64_t)(int64_t)*(int16_t*)avalue[i]; break;
        case FFI_TYPE_UINT32:
        case FFI_TYPE_SINT32:  args[i] = *(uint32_t*)avalue[i]; break;
        default:               args[i] = *(uint64_t*)avalue[i]; break;
        }
    }
    ffi_call_asm(fn, args, n, rvalue);
}

/* ---- Closure support ---- */
struct ffi_closure {
    ffi_cif*         cif;
    ffi_closure_fun  fun;
    void*            user_data;
    /* Trampoline code (self-modifying code to call ffi_closure_call) */
    uint8_t          tramp[24];
};

/* Global trampoline called by closure code */
void ffi_closure_call(ffi_closure* cl, uint64_t* args, void* rval) {
    /* Re-build void** array from raw uint64_t[] */
    void* avalue[32];
    unsigned n = cl->cif->nargs < 32 ? cl->cif->nargs : 32;
    for (unsigned i = 0; i < n; i++) avalue[i] = &args[i];
    cl->fun(cl->cif, rval, n ? avalue : NULL, cl->user_data);
}

void* ffi_closure_alloc(size_t size, void** code) {
    /* In a freestanding env we use the heap (no mmap/mprotect for exec) */
    void* p = malloc(size < sizeof(ffi_closure) ? sizeof(ffi_closure) : size);
    if (!p) return NULL;
    memset(p, 0, size < sizeof(ffi_closure) ? sizeof(ffi_closure) : size);
    if (code) *code = p; /* code pointer == data pointer (no W^X here) */
    return p;
}

void ffi_closure_free(void* closure) {
    free(closure);
}

ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
                                ffi_closure_fun fun, void* user_data,
                                void* codeloc) {
    (void)codeloc;
    if (!closure || !cif || !fun) return FFI_BAD_TYPEDEF;
    closure->cif       = cif;
    closure->fun       = fun;
    closure->user_data = user_data;
    return FFI_OK;
}
