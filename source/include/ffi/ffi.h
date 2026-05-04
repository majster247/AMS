#ifndef _AMS_FFI_H
#define _AMS_FFI_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI,
} ffi_status;

typedef enum {
    FFI_DEFAULT_ABI = 2,
    FFI_SYSV = 1,
    FFI_UNIX64 = 2,
} ffi_abi;

typedef struct {
    size_t size;
    unsigned short alignment;
    unsigned short type;
    struct _ffi_type** elements;
} ffi_type;

extern ffi_type ffi_type_void;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_double;
extern ffi_type ffi_type_pointer;

typedef struct {
    ffi_abi abi;
    unsigned nargs;
    ffi_type** arg_types;
    ffi_type* rtype;
    unsigned bytes;
    unsigned flags;
} ffi_cif;

typedef void (*ffi_closure_fun)(ffi_cif*, void*, void**, void*);

typedef struct {
    char tramp[24];
    ffi_cif* cif;
    ffi_closure_fun fun;
    void* user_data;
} ffi_closure;

ffi_status ffi_prep_cif(ffi_cif* cif, ffi_abi abi, unsigned int nargs,
                         ffi_type* rtype, ffi_type** atypes);
void ffi_call(ffi_cif* cif, void (*fn)(void), void* rvalue, void** avalue);
ffi_closure* ffi_closure_alloc(size_t size, void** code);
void ffi_closure_free(ffi_closure* closure);
ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
                                 ffi_closure_fun fun, void* user_data,
                                 void* code_loc);

#endif
