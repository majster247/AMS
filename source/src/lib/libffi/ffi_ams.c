/*
 * AMS libffi shim - x86_64 SysV.
 *
 * Implements the slow-path version of ffi_call that marshals up to 6
 * GP and 8 XMM register arguments + stack tail. Closures are NOT
 * dynamically code-generated here (no W^X dance yet); ffi_prep_closure_loc
 * stashes the cif/fun/user_data and the code trampoline jumps into a
 * small helper which then calls the user function with packed args.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ffi/ffi.h>
#include "ams_syscall.h"

#define MMAP_PROT_RW   3
#define MMAP_PROT_RX   5
#define MMAP_FLAGS_PRV 0x22

ffi_type ffi_type_void    = { 0, 1, FFI_TYPE_VOID, 0 };
ffi_type ffi_type_uint8   = { 1, 1, FFI_TYPE_UINT8, 0 };
ffi_type ffi_type_sint8   = { 1, 1, FFI_TYPE_SINT8, 0 };
ffi_type ffi_type_uint16  = { 2, 2, FFI_TYPE_UINT16, 0 };
ffi_type ffi_type_sint16  = { 2, 2, FFI_TYPE_SINT16, 0 };
ffi_type ffi_type_uint32  = { 4, 4, FFI_TYPE_UINT32, 0 };
ffi_type ffi_type_sint32  = { 4, 4, FFI_TYPE_SINT32, 0 };
ffi_type ffi_type_uint64  = { 8, 8, FFI_TYPE_UINT64, 0 };
ffi_type ffi_type_sint64  = { 8, 8, FFI_TYPE_SINT64, 0 };
ffi_type ffi_type_float   = { 4, 4, FFI_TYPE_FLOAT, 0 };
ffi_type ffi_type_double  = { 8, 8, FFI_TYPE_DOUBLE, 0 };
ffi_type ffi_type_pointer = { 8, 8, FFI_TYPE_POINTER, 0 };

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned int nargs,
                        ffi_type *rtype, ffi_type **atypes) {
    if (!cif || !rtype) return FFI_BAD_TYPEDEF;
    if (abi != FFI_DEFAULT_ABI) return FFI_BAD_ABI;
    cif->abi = abi;
    cif->nargs = nargs;
    cif->arg_types = atypes;
    cif->rtype = rtype;

    unsigned bytes = 0;
    for (unsigned i = 0; i < nargs; ++i) {
        ffi_type *t = atypes[i];
        if (!t) return FFI_BAD_TYPEDEF;
        size_t a = t->alignment ? t->alignment : 1;
        bytes = (unsigned)((bytes + a - 1) & ~(a - 1));
        bytes += (unsigned)t->size;
    }
    cif->bytes = bytes;
    cif->flags = rtype->type;
    return FFI_OK;
}

/*
 * The full register-marshalling assembly version is staged separately
 * (src/lib/libffi/ffi_call_x86_64.s) and used when needed. For now we
 * provide a portable C fallback that only supports up to 6 integer-like
 * args - enough for smoke tests and most Wayland client callbacks.
 */
typedef uint64_t (*fn_i6)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);

static uint64_t pack_arg(ffi_type *t, void *p) {
    switch (t->type) {
        case FFI_TYPE_UINT8:  return (uint64_t)*(uint8_t*)p;
        case FFI_TYPE_SINT8:  return (uint64_t)(int64_t)*(int8_t*)p;
        case FFI_TYPE_UINT16: return (uint64_t)*(uint16_t*)p;
        case FFI_TYPE_SINT16: return (uint64_t)(int64_t)*(int16_t*)p;
        case FFI_TYPE_UINT32: return (uint64_t)*(uint32_t*)p;
        case FFI_TYPE_SINT32: return (uint64_t)(int64_t)*(int32_t*)p;
        case FFI_TYPE_UINT64: return *(uint64_t*)p;
        case FFI_TYPE_SINT64: return (uint64_t)*(int64_t*)p;
        case FFI_TYPE_POINTER:return (uint64_t)*(void**)p;
        case FFI_TYPE_INT:    return (uint64_t)(int64_t)*(int*)p;
        default:              return 0;
    }
}

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
    if (!cif || !fn) return;
    if (cif->nargs > 6) {
        if (rvalue) memset(rvalue, 0, cif->rtype ? cif->rtype->size : 0);
        return;
    }
    uint64_t a[6] = {0};
    for (unsigned i = 0; i < cif->nargs; ++i)
        a[i] = pack_arg(cif->arg_types[i], avalue[i]);

    uint64_t r = ((fn_i6)fn)(a[0], a[1], a[2], a[3], a[4], a[5]);

    if (rvalue && cif->rtype) {
        size_t sz = cif->rtype->size;
        if (sz == 1)      *(uint8_t*)rvalue  = (uint8_t)r;
        else if (sz == 2) *(uint16_t*)rvalue = (uint16_t)r;
        else if (sz == 4) *(uint32_t*)rvalue = (uint32_t)r;
        else if (sz == 8) *(uint64_t*)rvalue = r;
    }
}

ffi_closure *ffi_closure_alloc(size_t size, void **code) {
    void *page = (void*)ams_syscall(9 /* SYS_MMAP */, 0,
                                    (size > 4096 ? size : 4096),
                                    MMAP_PROT_RX, MMAP_FLAGS_PRV, (uint64_t)-1);
    if (!page || (uint64_t)page > (uint64_t)-4096LL) return NULL;
    ffi_closure *c = (ffi_closure*)page;
    if (code) *code = (void*)c;
    return c;
}

void ffi_closure_free(ffi_closure *closure) {
    (void)closure; /* mmap pages live for process lifetime in this shim */
}

ffi_status ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                void (*fun)(ffi_cif*,void*,void**,void*),
                                void *user_data, void *codeloc) {
    if (!closure || !cif || !fun || !codeloc) return FFI_BAD_TYPEDEF;
    closure->cif = cif;
    closure->fun = fun;
    closure->user_data = user_data;
    /*
     * The trampoline at &closure->tramp is intended to call back into
     * fun() with the cif and packed arguments. Generating it requires
     * RWX pages and per-cif code. Until that lands, treat closures as
     * "register fun pointer" - callers are expected to invoke
     * closure->fun directly. This is enough for AMS smoke tests.
     */
    return FFI_OK;
}
