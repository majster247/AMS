/* src/lib/stubs.c - Atrapy dla TCC w AMS-1 */
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>

static void out_ch(char *dst, size_t size, size_t *pos, char c) {
    if (*pos + 1 < size) dst[*pos] = c;
    (*pos)++;
}

static void out_str(char *dst, size_t size, size_t *pos, const char *s) {
    if (!s) s = "(null)";
    while (*s) out_ch(dst, size, pos, *s++);
}

static void out_u64(char *dst, size_t size, size_t *pos, unsigned long long v, int base, int upper) {
    char tmp[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) {
        out_ch(dst, size, pos, '0');
        return;
    }
    while (v && i < (int)sizeof(tmp)) {
        tmp[i++] = digits[v % (unsigned)base];
        v /= (unsigned)base;
    }
    while (i--) out_ch(dst, size, pos, tmp[i]);
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int parse_uint(const char **pp) {
    int v = 0;
    const char *p = *pp;
    while (is_digit(*p)) {
        v = v * 10 + (*p - '0');
        p++;
    }
    *pp = p;
    return v;
}

static int out_u64_to_buf(char *tmp, int tmp_sz, unsigned long long v, int base, int upper) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) {
        if (tmp_sz > 1) tmp[i++] = '0';
        tmp[i] = '\0';
        return i;
    }
    char rev[64];
    int r = 0;
    while (v && r < (int)sizeof(rev)) {
        rev[r++] = digits[v % (unsigned)base];
        v /= (unsigned)base;
    }
    while (r > 0 && i + 1 < tmp_sz) {
        tmp[i++] = rev[--r];
    }
    tmp[i] = '\0';
    return i;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    size_t pos = 0;
    if (!str || size == 0) return 0;

    for (const char *p = format; *p; ++p) {
        if (*p != '%') {
            out_ch(str, size, &pos, *p);
            continue;
        }
        ++p;
        if (!*p) break;

        int zero_pad = 0;
        int width = 0;
        int precision = -1;

        if (*p == '0') {
            zero_pad = 1;
            ++p;
        }
        if (is_digit(*p)) {
            width = parse_uint(&p);
        }
        if (*p == '.') {
            ++p;
            precision = parse_uint(&p);
        }

        switch (*p) {
            case '%': out_ch(str, size, &pos, '%'); break;
            case 'c': out_ch(str, size, &pos, (char)va_arg(ap, int)); break;
            case 's': {
                const char *s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                int n = 0;
                while (s[n]) n++;
                int lim = (precision >= 0 && precision < n) ? precision : n;
                for (int i = 0; i < lim; ++i) out_ch(str, size, &pos, s[i]);
                break;
            }
            case 'd':
            case 'i': {
                long v = va_arg(ap, int);
                unsigned long long uv = (unsigned long long)((v < 0) ? -v : v);
                char num[64];
                int n = out_u64_to_buf(num, sizeof(num), uv, 10, 0);
                int need_zeros = 0;
                if (precision > n) need_zeros = precision - n;
                int sign = (v < 0) ? 1 : 0;
                int total = sign + need_zeros + n;
                char padc = (zero_pad && precision < 0) ? '0' : ' ';
                while (width > total) { out_ch(str, size, &pos, padc); width--; }
                if (sign) out_ch(str, size, &pos, '-');
                while (need_zeros-- > 0) out_ch(str, size, &pos, '0');
                for (int i = 0; i < n; ++i) out_ch(str, size, &pos, num[i]);
                break;
            }
            case 'u':
            case 'x':
            case 'X': {
                unsigned long long v = va_arg(ap, unsigned int);
                int base = (*p == 'u') ? 10 : 16;
                int upper = (*p == 'X') ? 1 : 0;
                char num[64];
                int n = out_u64_to_buf(num, sizeof(num), v, base, upper);
                int need_zeros = 0;
                if (precision > n) need_zeros = precision - n;
                int total = need_zeros + n;
                char padc = (zero_pad && precision < 0) ? '0' : ' ';
                while (width > total) { out_ch(str, size, &pos, padc); width--; }
                while (need_zeros-- > 0) out_ch(str, size, &pos, '0');
                for (int i = 0; i < n; ++i) out_ch(str, size, &pos, num[i]);
                break;
            }
            case 'p': {
                unsigned long long v = (unsigned long long)(size_t)va_arg(ap, void*);
                out_str(str, size, &pos, "0x");
                out_u64(str, size, &pos, v, 16, 0);
                break;
            }
            default:
                out_ch(str, size, &pos, '%');
                out_ch(str, size, &pos, *p);
                break;
        }
    }

    if (size > 0) {
        size_t end = (pos < size - 1) ? pos : (size - 1);
        str[end] = '\0';
    }
    return (int)pos;
}

/* --- LibC Stubs (To co normalnie jest w stdio/stdlib) --- */
int vfprintf(FILE *stream, const char *format, va_list ap) {
    char buf[2048];
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    if (n < 0) return n;

    int fd = 2;
    if (stream) {
        FILE *f = (FILE*)stream;
        fd = f->fd;
        if (fd < 0) fd = 2;
    }

    size_t to_write = (size_t)n;
    if (to_write >= sizeof(buf)) to_write = sizeof(buf) - 1;
    write(fd, buf, to_write);
    return n;
}
int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int rc = vsnprintf(str, size, format, ap);
    va_end(ap);
    return rc;
}
int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int rc = vsnprintf(str, (size_t)-1, format, ap);
    va_end(ap);
    return rc;
}
FILE *freopen(const char *filename, const char *mode, FILE *stream) {
    (void)filename;
    (void)mode;
    return stream;
}
//int atoi(const char *nptr) { return 0; }
long double strtold(const char *nptr, char **endptr) { return 0.0; }
long time(long *tloc) { return 0; }
int gettimeofday(void *tv, void *tz) { return 0; }
//char *realpath(const char *path, char *resolved_path) { return (char*)path; }
int mprotect(void *addr, size_t len, int prot) { return 0; }
int mkdir(const char *path, unsigned int mode) { (void)path; (void)mode; return 0; }
int usleep(unsigned int usec) { (void)usec; return 0; }
double fabs(double x) { return x < 0.0 ? -x : x; }
float fabsf(float x) { return x < 0.0f ? -x : x; }

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