#ifndef _SETJMP_H
#define _SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

// Rezerwujemy 64 bajty (8 rejestrów 64-bitowych), 
// co wystarcza dla standardowego setjmp na x86_64.
typedef long long jmp_buf[8];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif

#endif  