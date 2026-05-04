/**
 * Additional POSIX/libc functions needed by wlroots and its dependency chain.
 * Provides: shm_open, shm_unlink, getenv, setenv, realpath, clock_gettime,
 * strdup, strerror, strtol, strtoul, qsort, ioctl, dlopen/dlsym stubs,
 * pthread stubs, etc.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

extern uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2,
                            uint64_t p3, uint64_t p4, uint64_t p5);
extern void* malloc(size_t size);
extern void  free(void* ptr);
extern int   write(int fd, const char* buf, int count);

/* ---- shm_open / shm_unlink ---- */

int shm_open(const char* name, int oflag, unsigned int mode) {
    (void)oflag; (void)mode;
    return (int)ams_syscall(319 /* SYS_MEMFD_CREATE */, (uint64_t)name, 0, 0, 0, 0);
}

int shm_unlink(const char* name) {
    (void)name;
    return 0;
}

/* ---- environment ---- */

static char* g_env_keys[64];
static char* g_env_vals[64];
static int   g_env_count = 0;

char* getenv(const char* name) {
    if (!name) return (char*)0;
    for (int i = 0; i < g_env_count; i++) {
        const char* k = g_env_keys[i];
        const char* n = name;
        while (*k && *n && *k == *n) { k++; n++; }
        if (*k == 0 && *n == 0) return g_env_vals[i];
    }
    /* hardcoded defaults for wlroots/wayland */
    const char* n = name;
    if (n[0]=='X'&&n[1]=='D'&&n[2]=='G'&&n[3]=='_'&&n[4]=='R'&&n[5]=='U'&&n[6]=='N'&&n[7]=='T') {
        return (char*)"/run/user/0";
    }
    if (n[0]=='W'&&n[1]=='A'&&n[2]=='Y'&&n[3]=='L'&&n[4]=='A'&&n[5]=='N'&&n[6]=='D'&&n[7]=='_'&&n[8]=='D') {
        return (char*)"wayland-0";
    }
    return (char*)0;
}

static int str_len(const char* s) { int n = 0; while (s[n]) n++; return n; }
static void str_copy(char* d, const char* s) { while (*s) *d++ = *s++; *d = 0; }

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) return -1;
    for (int i = 0; i < g_env_count; i++) {
        const char* k = g_env_keys[i];
        const char* n = name;
        while (*k && *n && *k == *n) { k++; n++; }
        if (*k == 0 && *n == 0) {
            if (!overwrite) return 0;
            free(g_env_vals[i]);
            int vl = str_len(value);
            g_env_vals[i] = (char*)malloc((size_t)vl + 1);
            str_copy(g_env_vals[i], value);
            return 0;
        }
    }
    if (g_env_count >= 64) return -1;
    int kl = str_len(name);
    int vl = str_len(value);
    g_env_keys[g_env_count] = (char*)malloc((size_t)kl + 1);
    str_copy(g_env_keys[g_env_count], name);
    g_env_vals[g_env_count] = (char*)malloc((size_t)vl + 1);
    str_copy(g_env_vals[g_env_count], value);
    g_env_count++;
    return 0;
}

int unsetenv(const char* name) {
    (void)name;
    return 0;
}

/* ---- realpath ---- */

char* realpath(const char* path, char* resolved_path) {
    if (!path) return (char*)0;
    if (!resolved_path) {
        int len = str_len(path);
        resolved_path = (char*)malloc((size_t)len + 1);
    }
    str_copy(resolved_path, path);
    return resolved_path;
}

/* ---- clock_gettime (userspace wrapper) ---- */

struct timespec_local {
    int64_t tv_sec;
    int64_t tv_nsec;
};

int clock_gettime(int clk_id, struct timespec_local* tp) {
    (void)clk_id;
    return (int)ams_syscall(228 /* SYS_CLOCK_GETTIME */, 0, (uint64_t)tp, 0, 0, 0);
}

/* ---- strdup ---- */

char* strdup(const char* s) {
    if (!s) return (char*)0;
    int len = str_len(s);
    char* d = (char*)malloc((size_t)len + 1);
    if (d) str_copy(d, s);
    return d;
}

char* strndup(const char* s, size_t n) {
    if (!s) return (char*)0;
    size_t len = 0;
    while (len < n && s[len]) len++;
    char* d = (char*)malloc(len + 1);
    if (d) {
        for (size_t i = 0; i < len; i++) d[i] = s[i];
        d[len] = 0;
    }
    return d;
}

/* ---- strerror ---- */

static const char* g_err_names[] = {
    "Success", "EPERM", "ENOENT", "ESRCH", "EINTR", "EIO", "ENXIO",
    "E2BIG", "ENOEXEC", "EBADF", "ECHILD", "EAGAIN", "ENOMEM",
    "EACCES", "EFAULT"
};

char* strerror(int errnum) {
    if (errnum >= 0 && errnum < 15) return (char*)g_err_names[errnum];
    return (char*)"Unknown error";
}

/* ---- strtol / strtoul ---- */

static int is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int is_digit_c(char c) { return c >= '0' && c <= '9'; }
static int is_xdigit(char c) {
    return is_digit_c(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static int xval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

long strtol(const char* nptr, char** endptr, int base) {
    if (!nptr) { if (endptr) *endptr = (char*)nptr; return 0; }
    while (is_space(*nptr)) nptr++;
    int neg = 0;
    if (*nptr == '-') { neg = 1; nptr++; }
    else if (*nptr == '+') nptr++;
    if (base == 0) {
        if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) { base = 16; nptr += 2; }
        else if (nptr[0] == '0') { base = 8; nptr++; }
        else base = 10;
    } else if (base == 16 && nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
        nptr += 2;
    }
    long result = 0;
    while (*nptr) {
        int d = xval(*nptr);
        if (d < 0 || d >= base) break;
        result = result * base + d;
        nptr++;
    }
    if (endptr) *endptr = (char*)nptr;
    return neg ? -result : result;
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    return (unsigned long)strtol(nptr, endptr, base);
}

long long strtoll(const char* nptr, char** endptr, int base) {
    return (long long)strtol(nptr, endptr, base);
}

unsigned long long strtoull(const char* nptr, char** endptr, int base) {
    return (unsigned long long)strtol(nptr, endptr, base);
}

double strtod(const char* nptr, char** endptr) {
    if (!nptr) { if (endptr) *endptr = (char*)nptr; return 0.0; }
    while (is_space(*nptr)) nptr++;
    int neg = 0;
    if (*nptr == '-') { neg = 1; nptr++; }
    else if (*nptr == '+') nptr++;
    double result = 0.0;
    while (is_digit_c(*nptr)) {
        result = result * 10.0 + (*nptr - '0');
        nptr++;
    }
    if (*nptr == '.') {
        nptr++;
        double frac = 0.1;
        while (is_digit_c(*nptr)) {
            result += (*nptr - '0') * frac;
            frac *= 0.1;
            nptr++;
        }
    }
    if (endptr) *endptr = (char*)nptr;
    return neg ? -result : result;
}

float strtof(const char* nptr, char** endptr) {
    return (float)strtod(nptr, endptr);
}

/* ---- qsort ---- */

static void swap_bytes(void* a, void* b, size_t size) {
    uint8_t* pa = (uint8_t*)a;
    uint8_t* pb = (uint8_t*)b;
    for (size_t i = 0; i < size; i++) {
        uint8_t t = pa[i]; pa[i] = pb[i]; pb[i] = t;
    }
}

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*)) {
    if (nmemb < 2) return;
    uint8_t* arr = (uint8_t*)base;
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - 1 - i; j++) {
            if (compar(arr + j * size, arr + (j + 1) * size) > 0) {
                swap_bytes(arr + j * size, arr + (j + 1) * size, size);
            }
        }
    }
}

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*)) {
    const uint8_t* arr = (const uint8_t*)base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = compar(key, arr + mid * size);
        if (c == 0) return (void*)(arr + mid * size);
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return (void*)0;
}

/* ---- ioctl (userspace wrapper) ---- */

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void* argp = va_arg(ap, void*);
    va_end(ap);
    return (int)ams_syscall(16 /* SYS_IOCTL */, (uint64_t)fd, request, (uint64_t)argp, 0, 0);
}

/* ---- dlopen / dlsym stubs ---- */

static char g_dlerror_msg[64] = "dlopen not supported in AMS";

void* dlopen(const char* filename, int flags) {
    (void)filename; (void)flags;
    return (void*)0;
}

int dlclose(void* handle) {
    (void)handle;
    return 0;
}

void* dlsym(void* handle, const char* symbol) {
    (void)handle; (void)symbol;
    return (void*)0;
}

char* dlerror(void) {
    return g_dlerror_msg;
}

/* ---- pthread stubs (single-threaded) ---- */

int pthread_create(uint64_t* thread, const void* attr,
                   void* (*start_routine)(void*), void* arg) {
    (void)thread; (void)attr; (void)start_routine; (void)arg;
    return -1;
}

int pthread_join(uint64_t thread, void** retval) {
    (void)thread; (void)retval;
    return 0;
}

int pthread_detach(uint64_t thread) { (void)thread; return 0; }
uint64_t pthread_self(void) { return 1; }

int pthread_mutex_init(void* mutex, const void* attr) { (void)mutex; (void)attr; return 0; }
int pthread_mutex_destroy(void* mutex) { (void)mutex; return 0; }
int pthread_mutex_lock(void* mutex) { (void)mutex; return 0; }
int pthread_mutex_unlock(void* mutex) { (void)mutex; return 0; }

int pthread_cond_init(void* cond, const void* attr) { (void)cond; (void)attr; return 0; }
int pthread_cond_destroy(void* cond) { (void)cond; return 0; }
int pthread_cond_wait(void* cond, void* mutex) { (void)cond; (void)mutex; return 0; }
int pthread_cond_signal(void* cond) { (void)cond; return 0; }
int pthread_cond_broadcast(void* cond) { (void)cond; return 0; }

int pthread_once(int* once_control, void (*init_routine)(void)) {
    if (*once_control == 0) { init_routine(); *once_control = 1; }
    return 0;
}

/* ---- locale stubs ---- */

char* setlocale(int category, const char* locale) {
    (void)category; (void)locale;
    return (char*)"C";
}

struct lconv {
    char* decimal_point;
    char* thousands_sep;
};
static struct lconv g_lconv = { (char*)".", (char*)"" };

struct lconv* localeconv(void) { return &g_lconv; }

/* ---- signal stubs ---- */

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
    (void)signum; (void)handler;
    return (sighandler_t)0;
}

/* ---- math stubs (software implementations) ---- */

double log(double x) {
    if (x <= 0.0) return -1.0e308;
    double result = 0.0;
    double y = (x - 1.0) / (x + 1.0);
    double y2 = y * y;
    double term = y;
    for (int i = 1; i < 50; i += 2) {
        result += term / (double)i;
        term *= y2;
    }
    return 2.0 * result;
}

double log2(double x) { return log(x) / 0.6931471805599453; }
double log10(double x) { return log(x) / 2.302585092994046; }

double exp(double x) {
    double sum = 1.0, term = 1.0;
    for (int i = 1; i < 30; i++) {
        term *= x / (double)i;
        sum += term;
    }
    return sum;
}

double pow(double base, double exponent) {
    if (exponent == 0.0) return 1.0;
    if (base == 0.0) return 0.0;
    int neg = 0;
    if (exponent < 0.0) { neg = 1; exponent = -exponent; }
    double result;
    long iexp = (long)exponent;
    if ((double)iexp == exponent && iexp >= 0 && iexp < 64) {
        result = 1.0;
        double b = base;
        long e = iexp;
        while (e > 0) { if (e & 1) result *= b; b *= b; e >>= 1; }
    } else {
        result = exp(exponent * log(base));
    }
    return neg ? (1.0 / result) : result;
}

double sqrt(double x) {
    if (x < 0.0) return -1.0;
    if (x == 0.0) return 0.0;
    double guess = x / 2.0;
    for (int i = 0; i < 50; i++) {
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

double ceil(double x) {
    long i = (long)x;
    if (x > (double)i) return (double)(i + 1);
    return (double)i;
}

double floor(double x) {
    long i = (long)x;
    if (x < (double)i) return (double)(i - 1);
    return (double)i;
}

double round(double x) {
    return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5);
}

double fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    return x - (double)((long)(x / y)) * y;
}

float sqrtf(float x) { return (float)sqrt((double)x); }
float ceilf(float x) { return (float)ceil((double)x); }
float floorf(float x) { return (float)floor((double)x); }
float roundf(float x) { return (float)round((double)x); }
float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
float powf(float b, float e) { return (float)pow((double)b, (double)e); }
float logf(float x) { return (float)log((double)x); }
float log2f(float x) { return (float)log2((double)x); }
float expf(float x) { return (float)exp((double)x); }

double sin(double x) {
    x = fmod(x, 6.283185307179586);
    double result = 0.0, term = x;
    for (int i = 1; i < 20; i++) {
        result += term;
        term *= -x * x / (double)(2 * i * (2 * i + 1));
    }
    return result;
}

double cos(double x) { return sin(x + 1.5707963267948966); }
double tan(double x) { double c = cos(x); return (c == 0.0) ? 1e308 : sin(x) / c; }

float sinf(float x) { return (float)sin((double)x); }
float cosf(float x) { return (float)cos((double)x); }
float tanf(float x) { return (float)tan((double)x); }

double atan2(double y, double x) {
    if (x > 0.0) {
        double r = y / x;
        double s = r, t = r, r2 = r * r;
        for (int i = 1; i < 20; i++) {
            t *= -r2;
            s += t / (2.0 * i + 1.0);
        }
        return s;
    }
    if (x < 0.0) return (y >= 0.0) ? 3.14159265358979 + atan2(y, -x) : -3.14159265358979 + atan2(y, -x);
    return (y > 0.0) ? 1.5707963267948966 : (y < 0.0) ? -1.5707963267948966 : 0.0;
}

float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }

/* ---- misc ---- */

int abs_int(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }

unsigned int sleep(unsigned int seconds) {
    struct timespec_local ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    ams_syscall(35 /* SYS_NANOSLEEP */, (uint64_t)&ts, 0, 0, 0, 0);
    return 0;
}

int nanosleep(const struct timespec_local* req, struct timespec_local* rem) {
    (void)rem;
    return (int)ams_syscall(35, (uint64_t)req, 0, 0, 0, 0);
}

/* ---- isatty ---- */
int isatty(int fd) {
    (void)fd;
    return 0;
}

/* ---- fileno ---- */
int fileno(void* stream) {
    (void)stream;
    return -1;
}

/* ---- abort ---- */
void abort(void) {
    ams_syscall(60 /* SYS_EXIT */, 134, 0, 0, 0, 0);
    while (1) {}
}
