#ifndef _FFI_H
#define _FFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF = 1,
    FFI_BAD_ABI = 2
} ffi_status;

typedef enum {
    FFI_TYPE_VOID = 0,
    FFI_TYPE_INT = 1,
    FFI_TYPE_FLOAT = 2,
    FFI_TYPE_DOUBLE = 3,
    FFI_TYPE_LONGDOUBLE = 4,
    FFI_TYPE_UINT8 = 5,
    FFI_TYPE_SINT8 = 6,
    FFI_TYPE_UINT16 = 7,
    FFI_TYPE_SINT16 = 8,
    FFI_TYPE_UINT32 = 9,
    FFI_TYPE_SINT32 = 10,
    FFI_TYPE_UINT64 = 11,
    FFI_TYPE_SINT64 = 12,
    FFI_TYPE_STRUCT = 13,
    FFI_TYPE_POINTER = 14
} ffi_type_kind;

typedef struct ffi_type {
    size_t size;
    unsigned short alignment;
    unsigned short type;
    struct ffi_type** elements;
} ffi_type;

typedef struct ffi_cif {
    unsigned int abi;
    unsigned int nargs;
    ffi_type** arg_types;
    ffi_type* rtype;
    unsigned int bytes;
    unsigned int flags;
} ffi_cif;

typedef void (*ffi_closure_fun)(ffi_cif* cif, void* ret, void** args, void* user_data);

#define FFI_DEFAULT_ABI 0

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

ffi_status ffi_prep_cif(
    ffi_cif* cif,
    unsigned int abi,
    unsigned int nargs,
    ffi_type* rtype,
    ffi_type** atypes
);

void ffi_call(
    ffi_cif* cif,
    void (*fn)(void),
    void* rvalue,
    void** avalue
);

#ifdef __cplusplus
}
#endif

#endif
