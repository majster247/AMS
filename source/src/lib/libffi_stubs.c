/**
 * Minimal libffi implementation for AMS.
 *
 * Provides enough to support libwayland's closure dispatch pattern.
 * On x86_64, closures are implemented as simple trampoline stubs
 * that call through to a C function pointer.
 */

#include "ffi.h"
#include <stdint.h>
#include <stddef.h>

extern void* malloc(size_t size);
extern void  free(void* ptr);
extern void* memset(void* dest, int ch, size_t n);
extern void* memcpy(void* dest, const void* src, size_t n);

/* standard type descriptors */
ffi_type ffi_type_void    = {0, 1, 0, (ffi_type**)0};
ffi_type ffi_type_uint8   = {1, 1, 1, (ffi_type**)0};
ffi_type ffi_type_sint8   = {1, 1, 2, (ffi_type**)0};
ffi_type ffi_type_uint16  = {2, 2, 3, (ffi_type**)0};
ffi_type ffi_type_sint16  = {2, 2, 4, (ffi_type**)0};
ffi_type ffi_type_uint32  = {4, 4, 5, (ffi_type**)0};
ffi_type ffi_type_sint32  = {4, 4, 6, (ffi_type**)0};
ffi_type ffi_type_uint64  = {8, 8, 7, (ffi_type**)0};
ffi_type ffi_type_sint64  = {8, 8, 8, (ffi_type**)0};
ffi_type ffi_type_float   = {4, 4, 9, (ffi_type**)0};
ffi_type ffi_type_double  = {8, 8, 10, (ffi_type**)0};
ffi_type ffi_type_pointer = {8, 8, 11, (ffi_type**)0};

ffi_status ffi_prep_cif(ffi_cif* cif, ffi_abi abi, unsigned int nargs,
    ffi_type* rtype, ffi_type** atypes)
{
    if (!cif) return FFI_BAD_TYPEDEF;
    cif->abi = abi;
    cif->nargs = nargs;
    cif->rtype = rtype;
    cif->arg_types = atypes;
    cif->bytes = 0;
    cif->flags = 0;
    return FFI_OK;
}

/**
 * Trampoline slot table.
 * Each slot holds a closure's function pointer + user data,
 * and the "codeloc" is just the address of the wrapper function.
 */
#define MAX_CLOSURES 256

static struct {
    ffi_closure* closure;
    int in_use;
} g_closure_slots[MAX_CLOSURES];

static void closure_trampoline(ffi_cif* cif, void* ret, void** args, void* user_data) {
    ffi_closure* c = (ffi_closure*)user_data;
    if (c && c->fun) c->fun(c->cif, ret, args, c->user_data);
}

void* ffi_closure_alloc(size_t size, void** code) {
    (void)size;
    for (int i = 0; i < MAX_CLOSURES; i++) {
        if (!g_closure_slots[i].in_use) {
            ffi_closure* c = (ffi_closure*)malloc(sizeof(ffi_closure));
            if (!c) return (void*)0;
            memset(c, 0, sizeof(ffi_closure));
            g_closure_slots[i].closure = c;
            g_closure_slots[i].in_use = 1;
            if (code) *code = (void*)closure_trampoline;
            return c;
        }
    }
    return (void*)0;
}

void ffi_closure_free(void* closure) {
    for (int i = 0; i < MAX_CLOSURES; i++) {
        if (g_closure_slots[i].in_use && g_closure_slots[i].closure == closure) {
            g_closure_slots[i].in_use = 0;
            free(closure);
            return;
        }
    }
    free(closure);
}

ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
    ffi_closure_fun fun, void* user_data, void* codeloc)
{
    (void)codeloc;
    if (!closure) return FFI_BAD_TYPEDEF;
    closure->cif = cif;
    closure->fun = fun;
    closure->user_data = user_data;
    return FFI_OK;
}

void ffi_call(ffi_cif* cif, void (*fn)(void), void* rvalue, void** avalue) {
    if (!cif || !fn) return;

    /* Simple implementation: supports up to 6 integer/pointer args (x86-64 ABI).
     * This is sufficient for wayland's typical usage pattern. */
    uint64_t args[6] = {0};
    for (unsigned i = 0; i < cif->nargs && i < 6; i++) {
        if (!avalue || !avalue[i]) continue;
        ffi_type* t = cif->arg_types[i];
        if (!t) continue;
        switch (t->size) {
            case 1: args[i] = *(uint8_t*)avalue[i]; break;
            case 2: args[i] = *(uint16_t*)avalue[i]; break;
            case 4: args[i] = *(uint32_t*)avalue[i]; break;
            case 8: args[i] = *(uint64_t*)avalue[i]; break;
            default: args[i] = *(uint64_t*)avalue[i]; break;
        }
    }

    typedef uint64_t (*fn6_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
    uint64_t ret = ((fn6_t)fn)(args[0], args[1], args[2], args[3], args[4], args[5]);

    if (rvalue && cif->rtype && cif->rtype->size > 0) {
        switch (cif->rtype->size) {
            case 1: *(uint8_t*)rvalue  = (uint8_t)ret; break;
            case 2: *(uint16_t*)rvalue = (uint16_t)ret; break;
            case 4: *(uint32_t*)rvalue = (uint32_t)ret; break;
            case 8: *(uint64_t*)rvalue = ret; break;
            default: *(uint64_t*)rvalue = ret; break;
        }
    }
}
