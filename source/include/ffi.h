#ifndef _FFI_H
#define _FFI_H

#include <stddef.h>
#include <stdint.h>

typedef struct _ffi_type {
    size_t size;
    unsigned short alignment;
    unsigned short type;
    struct _ffi_type **elements;
} ffi_type;

typedef struct _ffi_cif {
    unsigned int abi;
    unsigned int nargs;
    ffi_type **arg_types;
    ffi_type *rtype;
} ffi_cif;

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF = 1,
    FFI_BAD_ABI = 2
} ffi_status;

#define FFI_DEFAULT_ABI 2

#ifdef __cplusplus
extern "C" {
#endif

extern ffi_type ffi_type_void;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_pointer;

ffi_status ffi_prep_cif(ffi_cif *cif, unsigned int abi, unsigned int nargs,
                        ffi_type *rtype, ffi_type **atypes);
void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue);

#ifdef __cplusplus
}
#endif

#endif
