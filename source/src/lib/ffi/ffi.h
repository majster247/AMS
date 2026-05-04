/* Minimal libffi port for AMS (x86_64 System V ABI only).
 * Implements enough of the libffi API for libwayland's dispatcher.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- type system ---- */
typedef unsigned int ffi_type_t;

#define FFI_TYPE_VOID       0
#define FFI_TYPE_INT        1
#define FFI_TYPE_FLOAT      2
#define FFI_TYPE_DOUBLE     3
#define FFI_TYPE_UINT8      5
#define FFI_TYPE_SINT8      6
#define FFI_TYPE_UINT16     7
#define FFI_TYPE_SINT16     8
#define FFI_TYPE_UINT32     9
#define FFI_TYPE_SINT32     10
#define FFI_TYPE_UINT64     11
#define FFI_TYPE_SINT64     12
#define FFI_TYPE_STRUCT     13
#define FFI_TYPE_POINTER    14

typedef struct _ffi_type {
    size_t           size;
    unsigned short   alignment;
    unsigned short   type;
    struct _ffi_type** elements; /* for structs */
} ffi_type;

/* Predefined type objects */
extern ffi_type ffi_type_void;
extern ffi_type ffi_type_pointer;
extern ffi_type ffi_type_sint;
extern ffi_type ffi_type_uint;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_double;

/* ---- ABI ---- */
typedef enum {
    FFI_SYSV    = 0,
    FFI_DEFAULT_ABI = FFI_SYSV
} ffi_abi;

/* ---- return codes ---- */
typedef enum {
    FFI_OK        = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI
} ffi_status;

/* ---- CIF (call interface) ---- */
typedef struct {
    ffi_abi    abi;
    unsigned   nargs;
    ffi_type** arg_types;
    ffi_type*  rtype;
    unsigned   bytes;
    unsigned   flags;
} ffi_cif;

ffi_status ffi_prep_cif(ffi_cif* cif, ffi_abi abi, unsigned nargs,
                        ffi_type* rtype, ffi_type** atypes);

void ffi_call(ffi_cif* cif, void (*fn)(void), void* rvalue, void** avalue);

/* ---- Closures ---- */
typedef struct ffi_closure ffi_closure;

typedef void (*ffi_closure_fun)(ffi_cif* cif, void* ret, void** args, void* user_data);

void* ffi_closure_alloc(size_t size, void** code);
void  ffi_closure_free(void* closure);

ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
                                ffi_closure_fun fun, void* user_data,
                                void* codeloc);

#ifdef __cplusplus
}
#endif
