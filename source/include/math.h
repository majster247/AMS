#ifndef _MATH_H
#define _MATH_H

#ifdef __cplusplus
extern "C" {
#endif

// Absolutne minimum dla TCC
double strtod(const char* nptr, char** endptr);
float strtof(const char* nptr, char** endptr);
long double strtold(const char* nptr, char** endptr);

#define HUGE_VAL (__builtin_huge_val())
#define NAN      (__builtin_nan(""))

#ifdef __cplusplus
}
#endif

#endif