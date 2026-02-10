/* src/lib/stubs.c - Atrapy dla TCC w AMS-1 */
#include <stddef.h>
#include <stdarg.h>

/* --- LibC Stubs (To co normalnie jest w stdio/stdlib) --- */
int vfprintf(void *stream, const char *format, va_list ap) { return 0; }
int vsnprintf(char *str, size_t size, const char *format, va_list ap) { return 0; }
int snprintf(char *str, size_t size, const char *format, ...) { return 0; }
int sprintf(char *str, const char *format, ...) { return 0; }
int fflush(void *stream) { return 0; }
void *freopen(const char *filename, const char *mode, void *stream) { return stream; }
//int atoi(const char *nptr) { return 0; }
long double strtold(const char *nptr, char **endptr) { return 0.0; }
long time(long *tloc) { return 0; }
int gettimeofday(void *tv, void *tz) { return 0; }
//char *realpath(const char *path, char *resolved_path) { return (char*)path; }
int mprotect(void *addr, size_t len, int prot) { return 0; }

/* --- Semafory (Wyłączamy wielowątkowość kompilacji) --- */
int sem_init(void *sem, int pshared, unsigned int value) { return 0; }
int sem_wait(void *sem) { return 0; }
int sem_post(void *sem) { return 0; }

/* --- TCC Debug / Coverage / Tools (Wewnętrzne funkcje TCC) --- */
/* Te funkcje normalnie są w tcc.c, ale przez nasze hacki zniknęły */
void tcc_debug_newfile(void *s1) {}
void tcc_debug_extern_sym(void *s1, void *sym, int sh_num, int sym_bind, int sym_type) {}
void tcc_debug_eincl(void *s1) {}
void tcc_debug_bincl(void *s1) {}
void tcc_debug_line(void *s1) {}
void tcc_debug_typedef(void *s1, void *sym) {}
void tcc_debug_stabn(void *s1, int type, int value) {}
void tcc_debug_fix_forw(void *s1, void *t) {}
void tcc_debug_funcstart(void *s1, void *sym) {}
void tcc_debug_prolog_epilog(void *s1, int value) {}
void tcc_debug_funcend(void *s1, int size) {}
void tcc_debug_start(void *s1) {}
void tcc_debug_end(void *s1) {}
void tcc_debug_new(void *s) {}
void tcc_add_debug_info(void *s1, void *s, void *e) {}

void tcc_tcov_check_line(void *s1, int start) {}
void tcc_tcov_block_begin(void *s1) {}
void tcc_tcov_block_end(void *s1, int line) {}
void tcc_tcov_reset_ind(void *s1) {}
void tcc_tcov_start(void *s1) {}
void tcc_tcov_end(void *s1) {}

void tcc_eh_frame_end(void *s1) {}
void tcc_eh_frame_start(void *s1) {}
void tcc_eh_frame_hdr(void *s1, int final) {}

void set_exception_handler(void) {}
void gen_makedeps(void *s, const char *target, const char *filename) {}