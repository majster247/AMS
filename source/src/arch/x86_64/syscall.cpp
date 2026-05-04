#include "ams_syscall.h"
#include "linux_syscalls.h"
#include "abi_types.h"
#include "kernel.h"
#include "vfs.h"
#include "task.h"
#include "vmm.h"
#include "gdt.h"
#include "graphics.h"
#include "drm_virt.h"
#include "evdev.h"
#include <stdint.h>

/* ---- forward declarations for new subsystems ---- */
extern "C" void drm_virt_init(void);
extern "C" void evdev_init(void);
extern "C" uint64_t drm_virt_ioctl(int fd, uint64_t request, uint64_t arg);
extern "C" uint64_t drm_virt_mmap_dumb(uint64_t map_offset, uint64_t length);
extern "C" int64_t  evdev_read(int device_idx, uint8_t* buf, uint64_t len);
extern "C" int      evdev_poll_ready(int device_idx);
extern "C" int      evdev_index_from_name(const char* name);

/* DRM mmap offset base — must match drm_virt.cpp */
#define DUMB_MMAP_BASE 0x100000000ULL

/* Marker stored in tar_data of evdev VFS nodes so we know device index */
#define FD_KIND_EVDEV  8
#define FD_KIND_DRM    9
#define FD_KIND_TIMER  10

typedef uint64_t (*syscall_fn)(registers* regs);
static constexpr uint64_t SYSCALL_TABLE_SIZE = 512;

extern "C" void syscall_entry(); // Funkcja w ASM, która jest punktem wejścia dla syscalli (w src/lib/syscall.s)

// --------- externy do k_memcpy, k_memset, kmalloc, kfree oraz k_strlen, k_strcpy, k_strstr (zdefiniowane w kernel.cpp) ---------
extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
extern "C" int k_strlen(const char* str);
extern "C" char* k_strcpy(char* dest, const char* src);
extern "C" char* k_strstr(const char* haystack, const char* needle);
// -------------------------------------------------------------------------------------------------------------
// write_msr i read_msr do obsługi MSR (Model Specific Registers) potrzebne do włączenia syscalli w procesorze
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// --------------------------------------------------------



static vfs_node* open_files[100];
static uint8_t fd_kind[100];      // 0=file/dir, 1=pipe_read, 2=pipe_write
static uint32_t fd_flags[100];    // F_GETFD/F_SETFD (FD_CLOEXEC)
static uint32_t fd_status[100];   // F_GETFL/F_SETFL
static void* fd_aux[100];         // np. pipe_state*
static uint64_t fd_pos[100];      // niezalezna pozycja per deskryptor
extern task* current_task;
extern task* kernel_task;
extern "C" uint64_t g_kernel_cr3;

// --- POMOCNICZE ---
static int get_free_fd() {
    for(int i = 3; i < 100; i++) {
        if (!open_files[i]) return i;
    }
    return -1;
}

static bool is_probably_user_ptr(const void* p) {
    uint64_t a = (uint64_t)p;
    return a >= 0x04000000ULL && a < 0x0000800000000000ULL;
}

static vfs_node g_root_dir;
static uint64_t g_rng_state = 0x9E3779B97F4A7C15ULL;

static void init_root_dir_node() {
    k_memset(&g_root_dir, 0, sizeof(g_root_dir));
    k_strcpy(g_root_dir.name, "/");
    g_root_dir.type = FS_DIRECTORY;
    g_root_dir.is_directory = true;
    g_root_dir.source = FS_TAR;
}

static uint64_t task_pid_or_default() {
    if (current_task && current_task->id) return current_task->id;
    return 1;
}

static uint64_t fast_random64() {
    // xorshift64* - szybki generator do /dev/urandom fallback
    uint64_t x = g_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng_state = x;
    return x * 2685821657736338717ULL;
}

struct pipe_state {
    uint8_t buf[4096];
    uint32_t rpos;
    uint32_t wpos;
    uint32_t count;
};

struct linux_sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

struct linux_msghdr {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    void* msg_iov;
    uint64_t msg_iovlen;
    void* msg_control;
    uint64_t msg_controllen;
    uint32_t msg_flags;
    uint32_t __pad1;
};

struct linux_cmsghdr {
    uint64_t cmsg_len;
    int32_t cmsg_level;
    int32_t cmsg_type;
};

struct unix_msg {
    uint8_t data[1024];
    uint32_t len;
    int16_t fds[8];
    uint8_t fd_count;
};

struct unix_socket_state {
    bool in_use;
    bool bound;
    bool listening;
    bool connected;
    int peer_fd;
    char path[108];
    uint8_t q_head;
    uint8_t q_tail;
    uint8_t q_count;
    unix_msg queue[16];
};

struct epoll_watch {
    int fd;
    uint32_t events;
    uint64_t data;
};

struct epoll_state {
    bool in_use;
    epoll_watch watches[64];
    uint32_t watch_count;
};

struct eventfd_state {
    uint64_t value;
    uint32_t flags;
};

static unix_socket_state g_unix_sockets[100];
static epoll_state g_epolls[32];
static uint64_t g_memfd_counter = 1;

static constexpr uint8_t FD_KIND_FILE = 0;
static constexpr uint8_t FD_KIND_PIPE_READ = 1;
static constexpr uint8_t FD_KIND_PIPE_WRITE = 2;
static constexpr uint8_t FD_KIND_SOCKET = 3;
static constexpr uint8_t FD_KIND_EPOLL = 4;
static constexpr uint8_t FD_KIND_EVENTFD = 5;
static constexpr uint8_t FD_KIND_DEV_NULL = 6;
static constexpr uint8_t FD_KIND_DEV_URANDOM = 7;

static int path_basename(const char* in, char* out, size_t out_sz) {
    if (!in || !out || out_sz < 2) return -22;
    const char* p = in;
    while (*p == '/') ++p;
    while (p[0] == '.' && p[1] == '/') p += 2;
    const char* last = p;
    for (const char* it = p; *it; ++it) {
        if (*it == '/') last = it + 1;
    }
    if (!*last) return -22;
    size_t n = 0;
    while (last[n] && n + 1 < out_sz) {
        out[n] = last[n];
        ++n;
    }
    out[n] = '\0';
    return 0;
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
    return (a < b) ? a : b;
}

static bool copy_user_path(const char* user_ptr, char* out, size_t out_sz) {
    if (!user_ptr || !out || out_sz < 2) return false;
    if (!is_probably_user_ptr(user_ptr)) return false;
    size_t i = 0;
    for (; i < out_sz - 1; ++i) {
        char c = user_ptr[i];
        out[i] = c;
        if (c == '\0') return true;
    }
    out[out_sz - 1] = '\0';
    return true;
}

static void dbg_path(const char* tag, const char* user_ptr, const char* copied_or_null) {
    write_serial_string("[SYSCALL][PATH] ");
    write_serial_string(tag);
    write_serial_string(" ptr=");
    write_serial_hex((uint64_t)user_ptr);
    if (copied_or_null) {
        write_serial_string(" val=\"");
        write_serial_string(copied_or_null);
        write_serial_string("\"");
    } else {
        write_serial_string(" val=<invalid>");
    }
    write_serial_string("\n");
}

static bool is_reasonable_path(const char* s) {
    if (!s || !s[0]) return false;
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

static void make_fallback_tmp_path(char* out, size_t out_sz, uint64_t key) {
    const char* prefix = "/__tmp_";
    const char* hex = "0123456789abcdef";
    size_t p = 0;
    for (; prefix[p] && p + 1 < out_sz; ++p) out[p] = prefix[p];
    for (int i = 15; i >= 0 && p + 1 < out_sz; --i) {
        out[p++] = hex[(key >> (i * 4)) & 0xF];
    }
    out[p] = '\0';
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static bool fd_is_readable(int fd) {
    if (fd < 0 || fd >= 100) return false;
    if (fd == 0) return true;
    if (!open_files[fd]) return false;
    if (fd_kind[fd] == FD_KIND_PIPE_READ) {
        pipe_state* ps = (pipe_state*)fd_aux[fd];
        return ps && ps->count > 0;
    }
    if (fd_kind[fd] == FD_KIND_PIPE_WRITE) return false;
    if (fd_kind[fd] == FD_KIND_SOCKET) {
        unix_socket_state* us = &g_unix_sockets[fd];
        return us->in_use && us->q_count > 0;
    }
    if (fd_kind[fd] == FD_KIND_EVENTFD) {
        eventfd_state* es = (eventfd_state*)fd_aux[fd];
        return es && es->value > 0;
    }
    if (fd_kind[fd] == FD_KIND_EVDEV) {
        int dev_idx = evdev_index_from_name(open_files[fd]->name);
        return dev_idx >= 0 && evdev_poll_ready(dev_idx) != 0;
    }
    return true;
}

static bool fd_is_writable(int fd) {
    if (fd < 0 || fd >= 100) return false;
    if (fd == 1 || fd == 2) return true;
    if (!open_files[fd]) return false;
    if (fd_kind[fd] == FD_KIND_PIPE_WRITE) {
        pipe_state* ps = (pipe_state*)fd_aux[fd];
        return ps && ps->count < sizeof(ps->buf);
    }
    if (fd_kind[fd] == FD_KIND_PIPE_READ) return false;
    if (fd_kind[fd] == FD_KIND_SOCKET) {
        unix_socket_state* us = &g_unix_sockets[fd];
        if (!us->in_use || !us->connected) return false;
        if (us->peer_fd < 0 || us->peer_fd >= 100) return false;
        unix_socket_state* peer = &g_unix_sockets[us->peer_fd];
        return peer->in_use && peer->q_count < 16;
    }
    return true;
}

static void clear_fd_slot(int fd) {
    if (fd < 0 || fd >= 100) return;
    if (!open_files[fd]) return;

    if (fd_kind[fd] == FD_KIND_SOCKET) {
        unix_socket_state* us = &g_unix_sockets[fd];
        if (us->in_use && us->peer_fd >= 0 && us->peer_fd < 100) {
            unix_socket_state* peer = &g_unix_sockets[us->peer_fd];
            if (peer->in_use && peer->peer_fd == fd) {
                peer->connected = false;
                peer->peer_fd = -1;
            }
        }
        k_memset(us, 0, sizeof(*us));
        us->peer_fd = -1;
    } else if (fd_kind[fd] == FD_KIND_EPOLL) {
        epoll_state* ep = (epoll_state*)fd_aux[fd];
        if (ep) {
            ep->in_use = false;
            ep->watch_count = 0;
        }
    } else if (fd_kind[fd] == FD_KIND_EVENTFD) {
        eventfd_state* es = (eventfd_state*)fd_aux[fd];
        if (es) kfree(es);
    }

    open_files[fd] = nullptr;
    fd_kind[fd] = 0;
    fd_flags[fd] = 0;
    fd_status[fd] = 0;
    fd_aux[fd] = nullptr;
    fd_pos[fd] = 0;
}

struct linux_iovec {
    void* iov_base;
    uint64_t iov_len;
};

struct linux_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct linux_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

struct linux_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

struct linux_epoll_event {
    uint32_t events;
    uint64_t data;
} __attribute__((packed));

static void write_user_cstr(char* dst, const char* src, uint64_t max_len) {
    if (!dst || !src || max_len == 0) return;
    uint64_t i = 0;
    for (; i + 1 < max_len && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

uint64_t sys_exit(registers* regs);

// --- IMPLEMENTACJE SYSCALLI ---

uint64_t sys_not_implemented(registers* regs) {
    write_serial_string("[SYSCALL] NOT IMPLEMENTED: ");
    write_serial_dec(regs->rax);
    write_serial_string("\n");
    return -38; // ENOSYS
}

uint64_t sys_read(registers* regs) {
    int fd = (int)regs->rdi;
    uint8_t* buf = (uint8_t*)regs->rsi;
    size_t count = regs->rdx;

    if (fd < 0 || fd >= 100) return -9; // EBADF
    if (fd == 0) return 0; // STDIN dummy

    vfs_node* f = open_files[fd];
    if (!f) return -9;

    if (fd_kind[fd] == FD_KIND_PIPE_READ) {
        pipe_state* ps = (pipe_state*)fd_aux[fd];
        if (!ps) return -9;
        uint64_t n = 0;
        while (n < count && ps->count) {
            buf[n++] = ps->buf[ps->rpos];
            ps->rpos = (ps->rpos + 1) % sizeof(ps->buf);
            ps->count--;
        }
        return n;
    }

    if (fd_kind[fd] == FD_KIND_EVENTFD) {
        eventfd_state* es = (eventfd_state*)fd_aux[fd];
        if (!es) return (uint64_t)-9;
        if (count < 8) return (uint64_t)-22;
        if (es->value == 0) return (uint64_t)-11;
        uint64_t v = es->value;
        es->value = 0;
        k_memcpy(buf, &v, 8);
        return 8;
    }

    if (fd_kind[fd] == FD_KIND_DEV_NULL) {
        return 0;
    }

    /* evdev device read — returns struct input_event records */
    if (fd_kind[fd] == FD_KIND_EVDEV) {
        int dev_idx = evdev_index_from_name(open_files[fd]->name);
        if (dev_idx < 0) return (uint64_t)-9;
        int64_t n = evdev_read(dev_idx, buf, count);
        return (n < 0) ? (uint64_t)-9 : (uint64_t)n;
    }

    /* DRM nodes are not readable via read() */
    if (fd_kind[fd] == FD_KIND_DRM) {
        return (uint64_t)-22;
    }

    if (fd_kind[fd] == FD_KIND_DEV_URANDOM) {
        size_t produced = 0;
        while (produced < count) {
            uint64_t r = fast_random64();
            size_t chunk = (count - produced > sizeof(r)) ? sizeof(r) : (count - produced);
            k_memcpy(buf + produced, &r, chunk);
            produced += chunk;
        }
        return produced;
    }

    size_t read_bytes = 0;
    if (f->tar_data) {
        uint64_t pos = fd_pos[fd];
        size_t available = (pos < f->size) ? (size_t)(f->size - pos) : 0;
        read_bytes = (count < available) ? count : available;
        k_memcpy(buf, f->tar_data + pos, read_bytes);
    } else if (f->read) {
        read_bytes = vfs_read(f, fd_pos[fd], count, buf);
    }
    
    fd_pos[fd] += read_bytes;
    return read_bytes;
}

uint64_t sys_lseek(registers* regs) {
    int fd = (int)regs->rdi;
    int64_t offset = (int64_t)regs->rsi;
    int whence = (int)regs->rdx;

    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9; // EBADF
    vfs_node* f = open_files[fd];

    int64_t base = 0;
    if (whence == 0) base = 0;                       // SEEK_SET
    else if (whence == 1) base = (int64_t)fd_pos[fd]; // SEEK_CUR
    else if (whence == 2) base = (int64_t)f->size;         // SEEK_END
    else return (uint64_t)-22; // EINVAL

    int64_t new_pos = base + offset;
    if (new_pos < 0) return (uint64_t)-22;
    fd_pos[fd] = (uint64_t)new_pos;
    return fd_pos[fd];
}

uint64_t sys_write(registers* regs) {
    int fd = (int)regs->rdi;
    uint8_t* buf = (uint8_t*)regs->rsi;
    size_t count = regs->rdx;

    if (fd == 1 || fd == 2) {
        for(size_t i=0; i<count; i++) write_serial_char(buf[i]);
        return count;
    }

    if (fd < 0 || fd >= 100 || !open_files[fd]) return -9;
    vfs_node* f = open_files[fd];

    if (fd_kind[fd] == FD_KIND_PIPE_WRITE) {
        pipe_state* ps = (pipe_state*)fd_aux[fd];
        if (!ps) return -9;
        uint64_t n = 0;
        while (n < count && ps->count < sizeof(ps->buf)) {
            ps->buf[ps->wpos] = buf[n++];
            ps->wpos = (ps->wpos + 1) % sizeof(ps->buf);
            ps->count++;
        }
        return n;
    }

    if (fd_kind[fd] == FD_KIND_EVENTFD) {
        eventfd_state* es = (eventfd_state*)fd_aux[fd];
        if (!es) return (uint64_t)-9;
        if (count < 8) return (uint64_t)-22;
        uint64_t v = 0;
        k_memcpy(&v, buf, 8);
        es->value += v;
        return 8;
    }

    if (fd_kind[fd] == FD_KIND_DEV_NULL || fd_kind[fd] == FD_KIND_DEV_URANDOM) {
        return count;
    }

    // Lazy allocation dla nowych plików (jak kupa.o)
    if (!f->tar_data && f->size == 0) {
        f->max_size = 512 * 1024;
        f->tar_data = (uint8_t*)kmalloc(f->max_size);
        k_memset(f->tar_data, 0, f->max_size);
    }

    if (fd_pos[fd] + count > f->max_size) count = f->max_size - fd_pos[fd];
    k_memcpy(f->tar_data + fd_pos[fd], buf, count);
    fd_pos[fd] += count;
    if (fd_pos[fd] > f->size) f->size = fd_pos[fd];

    return count;
}

uint64_t sys_readv(registers* regs) {
    int fd = (int)regs->rdi;
    linux_iovec* iov = (linux_iovec*)regs->rsi;
    int iovcnt = (int)regs->rdx;
    if (iovcnt < 0 || iovcnt > 64) return (uint64_t)-22;

    uint64_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (!iov[i].iov_base || iov[i].iov_len == 0) continue;
        registers r{};
        r.rdi = (uint64_t)fd;
        r.rsi = (uint64_t)iov[i].iov_base;
        r.rdx = iov[i].iov_len;
        uint64_t rc = sys_read(&r);
        if ((int64_t)rc < 0) return total ? total : rc;
        total += rc;
        if (rc < iov[i].iov_len) break;
    }
    return total;
}

uint64_t sys_writev(registers* regs) {
    int fd = (int)regs->rdi;
    linux_iovec* iov = (linux_iovec*)regs->rsi;
    int iovcnt = (int)regs->rdx;
    if (iovcnt < 0 || iovcnt > 64) return (uint64_t)-22;

    uint64_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (!iov[i].iov_base || iov[i].iov_len == 0) continue;
        registers r{};
        r.rdi = (uint64_t)fd;
        r.rsi = (uint64_t)iov[i].iov_base;
        r.rdx = iov[i].iov_len;
        uint64_t rc = sys_write(&r);
        if ((int64_t)rc < 0) return total ? total : rc;
        total += rc;
        if (rc < iov[i].iov_len) break;
    }
    return total;
}

/* Helper: compare two C strings (without external strcmp where it may not exist) */
static bool streq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

/* Helper: check if path starts with prefix */
static bool startswith(const char* s, const char* pfx) {
    while (*pfx) { if (*s++ != *pfx++) return false; }
    return true;
}

static int open_common(char* path_buf, int flags) {
    vfs_node* node = nullptr;
    bool pseudo_dev_null     = streq(path_buf, "/dev/null");
    bool pseudo_dev_urandom  = streq(path_buf, "/dev/urandom");
    bool pseudo_dev_drm      = streq(path_buf, "/dev/dri/card0");
    bool pseudo_dev_evdev0   = streq(path_buf, "/dev/input/event0");
    bool pseudo_dev_evdev1   = streq(path_buf, "/dev/input/event1");
    bool is_shm = startswith(path_buf, "/dev/shm/") && path_buf[9] != '\0';

    uint8_t kind = FD_KIND_FILE;

    if ((path_buf[0] == '/' && path_buf[1] == '\0') ||
        (path_buf[0] == '.' && path_buf[1] == '\0')) {
        node = &g_root_dir;
    } else if (pseudo_dev_null) {
        node = &g_root_dir; kind = FD_KIND_DEV_NULL;
    } else if (pseudo_dev_urandom) {
        node = &g_root_dir; kind = FD_KIND_DEV_URANDOM;
    } else if (pseudo_dev_drm) {
        node = vfs_find("card0");
        if (!node) node = &g_root_dir;
        kind = FD_KIND_DRM;
    } else if (pseudo_dev_evdev0) {
        node = vfs_find("event0");
        if (!node) node = &g_root_dir;
        kind = FD_KIND_EVDEV;
    } else if (pseudo_dev_evdev1) {
        node = vfs_find("event1");
        if (!node) node = &g_root_dir;
        kind = FD_KIND_EVDEV;
    } else if (is_shm) {
        /* /dev/shm/<name>: find or create a memfd-style node */
        const char* shm_name = path_buf + 9;
        node = vfs_find(shm_name);
        if (!node && (flags & 0x40)) { /* O_CREAT */
            node = (vfs_node*)kmalloc(sizeof(vfs_node));
            k_memset(node, 0, sizeof(vfs_node));
            k_strcpy(node->name, shm_name);
            node->type     = FS_FILE;
            node->source   = FS_TAR;
            node->max_size = 4096;
            node->tar_data = (uint8_t*)kmalloc(4096);
            k_memset(node->tar_data, 0, 4096);
            node->next = vfs_root;
            vfs_root   = node;
        }
        kind = FD_KIND_FILE;
    } else {
        node = vfs_find(path_buf);
        if (!node && (flags & 0x40)) { /* O_CREAT */
            const char* normalized = path_buf;
            while (*normalized == '/') normalized++;
            while (normalized[0] == '.' && normalized[1] == '/') normalized += 2;
            const char* last = normalized;
            for (const char* p = normalized; *p; ++p) {
                if (*p == '/') last = p + 1;
            }
            if (*last) normalized = last;
            node = (vfs_node*)kmalloc(sizeof(vfs_node));
            k_memset(node, 0, sizeof(vfs_node));
            k_strcpy(node->name, normalized);
            node->next = vfs_root;
            vfs_root = node;
        }
    }

    if (!node) return -2; /* ENOENT */

    if ((flags & 0x200) && kind == FD_KIND_FILE) { /* O_TRUNC */
        node->size = 0;
        node->current_pos = 0;
    }

    int fd = get_free_fd();
    if (fd == -1) return -24; /* EMFILE */

    open_files[fd] = node;
    fd_kind[fd]   = kind;
    fd_flags[fd]  = 0;
    fd_status[fd] = (uint32_t)flags;
    fd_aux[fd]    = nullptr;
    fd_pos[fd]    = 0;
    return fd;
}

uint64_t sys_open(registers* regs) {
    const char* path = (const char*)regs->rdi;
    int flags = (int)regs->rsi;
    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) {
        dbg_path("open", path, nullptr);
        return -14; // EFAULT
    }
    if (!is_reasonable_path(path_buf)) {
        make_fallback_tmp_path(path_buf, sizeof(path_buf), (uint64_t)path);
    }
    dbg_path("open", path, path_buf);
    return (uint64_t)(int64_t)open_common(path_buf, flags);
}

uint64_t sys_openat(registers* regs) {
    (void)regs->rdi; // dirfd
    const char* path = (const char*)regs->rsi;
    int flags = (int)regs->rdx;
    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) {
        dbg_path("openat", path, nullptr);
        return -14; // EFAULT
    }
    if (!is_reasonable_path(path_buf)) {
        make_fallback_tmp_path(path_buf, sizeof(path_buf), (uint64_t)path);
    }
    dbg_path("openat", path, path_buf);
    return (uint64_t)(int64_t)open_common(path_buf, flags);
}


uint64_t sys_access(registers* regs) {
    const char* path = (const char*)regs->rdi;
    (void)regs->rsi; // mode
    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) {
        dbg_path("access", path, nullptr);
        return -14;
    }
    dbg_path("access", path, path_buf);
    return vfs_find(path_buf) ? 0 : (uint64_t)-2; // ENOENT
}

uint64_t sys_faccessat(registers* regs) {
    (void)regs->rdi; // dirfd
    const char* path = (const char*)regs->rsi;
    (void)regs->rdx; // mode
    (void)regs->r10; // flags
    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) {
        dbg_path("faccessat", path, nullptr);
        return -14;
    }
    dbg_path("faccessat", path, path_buf);
    if (path_buf[0] == '\0') return (uint64_t)-2;
    return vfs_find(path_buf) ? 0 : (uint64_t)-2;
}

uint64_t sys_newfstatat(registers* regs) {
    (void)regs->rdi; // dirfd
    const char* path = (const char*)regs->rsi;
    uint8_t* statbuf = (uint8_t*)regs->rdx;
    (void)regs->r10; // flags
    if (!statbuf) return -14; // EFAULT

    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) {
        dbg_path("newfstatat", path, nullptr);
        return -14;
    }
    dbg_path("newfstatat", path, path_buf);
    if (path_buf[0] == '\0') return -2;

    vfs_node* node = vfs_find(path_buf);
    if (!node) return -2;

    // Minimalny Linux x86_64 struct stat layout (wystarczający dla TCC probes).
    k_memset(statbuf, 0, 144);
    uint64_t nlink = 1;
    uint32_t mode = node->is_directory ? 0040755u : 0100644u;
    uint64_t size = node->size;
    k_memcpy(statbuf + 16, &nlink, sizeof(nlink)); // st_nlink
    k_memcpy(statbuf + 24, &mode, sizeof(mode));   // st_mode
    k_memcpy(statbuf + 48, &size, sizeof(size));   // st_size
    return 0;
}

uint64_t sys_stat(registers* regs) {
    registers r = *regs;
    r.rdi = (uint64_t)-100; // AT_FDCWD
    r.rsi = regs->rdi;      // pathname
    r.rdx = regs->rsi;      // statbuf
    r.r10 = 0;
    return sys_newfstatat(&r);
}

uint64_t sys_fstat(registers* regs) {
    int fd = (int)regs->rdi;
    uint8_t* statbuf = (uint8_t*)regs->rsi;
    if (!statbuf) return -14; // EFAULT
    if (fd < 0 || fd >= 100 || !open_files[fd]) return -9; // EBADF

    vfs_node* node = open_files[fd];
    k_memset(statbuf, 0, 144);
    uint64_t nlink = 1;
    uint32_t mode = node->is_directory ? 0040755u : 0100644u;
    uint64_t size = node->size;
    k_memcpy(statbuf + 16, &nlink, sizeof(nlink));
    k_memcpy(statbuf + 24, &mode, sizeof(mode));
    k_memcpy(statbuf + 48, &size, sizeof(size));
    return 0;
}

uint64_t sys_readlink(registers* regs) {
    const char* path = (const char*)regs->rdi;
    char* out = (char*)regs->rsi;
    size_t out_sz = (size_t)regs->rdx;
    if (!path || !out || out_sz == 0) return (uint64_t)-14; // EFAULT
    if (!is_probably_user_ptr(path) || !is_probably_user_ptr(out)) return (uint64_t)-14;

    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) return (uint64_t)-14;

    // Minimalna zgodność dla toolchaina: /proc/self/exe -> /tools/compiler/tcc
    const char expected[] = "/proc/self/exe";
    int i = 0;
    for (; expected[i] && path_buf[i] && expected[i] == path_buf[i]; ++i) {}
    if (expected[i] != '\0' || path_buf[i] != '\0') return (uint64_t)-2; // ENOENT

    const char target[] = "/tools/compiler/tcc";
    size_t n = sizeof(target) - 1; // bez '\0', jak w Linux readlink
    if (n > out_sz) n = out_sz;
    k_memcpy(out, target, n);
    return (uint64_t)n;
}

uint64_t sys_ioctl(registers* regs) {
    int fd = (int)regs->rdi;
    uint64_t request = regs->rsi;
    uint64_t argp    = regs->rdx;
    if (fd == 0 || fd == 1 || fd == 2) return (uint64_t)-25; // ENOTTY
    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9; // EBADF

    /* Route DRM ioctls to the virtual DRM driver */
    if (fd_kind[fd] == FD_KIND_DRM) {
        return drm_virt_ioctl(fd, request, argp);
    }

    /* EVDEV ioctl stubs (EVIOCGNAME, EVIOCGID, EVIOCGBIT) — return minimal info */
    if (fd_kind[fd] == FD_KIND_EVDEV) {
        uint8_t nr = (uint8_t)((request >> 8) & 0xFF);
        if (nr == 0x06) { /* EVIOCGNAME */
            uint64_t len = (request >> 16) & 0xFF;
            if (argp && is_probably_user_ptr((void*)argp) && len > 0) {
                const char* name = "ams-input";
                uint64_t n = k_strlen(name);
                if (n >= len) n = len - 1;
                k_memcpy((void*)argp, name, n + 1);
            }
            return 9;
        }
        return 0; /* accept all other evdev ioctls silently */
    }

    return (uint64_t)-25; // ENOTTY
}

uint64_t sys_dup(registers* regs) {
    int oldfd = (int)regs->rdi;
    if (oldfd < 0 || oldfd >= 100 || !open_files[oldfd]) return (uint64_t)-9;
    int newfd = get_free_fd();
    if (newfd < 0) return (uint64_t)-24;
    open_files[newfd] = open_files[oldfd];
    fd_kind[newfd] = fd_kind[oldfd];
    fd_flags[newfd] = fd_flags[oldfd];
    fd_status[newfd] = fd_status[oldfd];
    fd_aux[newfd] = fd_aux[oldfd];
    fd_pos[newfd] = fd_pos[oldfd];
    return (uint64_t)newfd;
}

uint64_t sys_dup2(registers* regs) {
    int oldfd = (int)regs->rdi;
    int newfd = (int)regs->rsi;
    if (oldfd < 0 || oldfd >= 100 || !open_files[oldfd]) return (uint64_t)-9;
    if (newfd < 0 || newfd >= 100) return (uint64_t)-9;
    open_files[newfd] = open_files[oldfd];
    fd_kind[newfd] = fd_kind[oldfd];
    fd_flags[newfd] = fd_flags[oldfd];
    fd_status[newfd] = fd_status[oldfd];
    fd_aux[newfd] = fd_aux[oldfd];
    fd_pos[newfd] = fd_pos[oldfd];
    return (uint64_t)newfd;
}

uint64_t sys_fcntl(registers* regs) {
    int fd = (int)regs->rdi;
    int cmd = (int)regs->rsi;
    uint64_t arg = regs->rdx;
    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9;

    switch (cmd) {
        case 1: // F_GETFD
            return fd_flags[fd];
        case 2: // F_SETFD
            fd_flags[fd] = (uint32_t)arg;
            return 0;
        case 3: // F_GETFL
            return fd_status[fd];
        case 4: // F_SETFL
            fd_status[fd] = (uint32_t)arg;
            return 0;
        default:
            return (uint64_t)-22;
    }
}

uint64_t sys_pipe2(registers* regs) {
    int* pipefd = (int*)regs->rdi;
    (void)regs->rsi; // flags
    if (!pipefd || !is_probably_user_ptr(pipefd)) return (uint64_t)-14;

    int fd0 = get_free_fd();
    if (fd0 < 0) return (uint64_t)-24;
    open_files[fd0] = &g_root_dir;

    int fd1 = -1;
    for (int i = fd0 + 1; i < 100; ++i) {
        if (!open_files[i]) { fd1 = i; break; }
    }
    if (fd1 < 0) {
        open_files[fd0] = nullptr;
        return (uint64_t)-24;
    }
    open_files[fd1] = &g_root_dir;

    pipe_state* ps = (pipe_state*)kmalloc(sizeof(pipe_state));
    if (!ps) {
        open_files[fd0] = nullptr;
        open_files[fd1] = nullptr;
        return (uint64_t)-12;
    }
    k_memset(ps, 0, sizeof(pipe_state));

    fd_kind[fd0] = 1; fd_kind[fd1] = 2;
    fd_flags[fd0] = fd_flags[fd1] = 0;
    fd_status[fd0] = 0; fd_status[fd1] = 1;
    fd_aux[fd0] = ps; fd_aux[fd1] = ps;

    pipefd[0] = fd0;
    pipefd[1] = fd1;
    return 0;
}

uint64_t sys_socket(registers* regs) {
    int domain = (int)regs->rdi;
    int type = (int)regs->rsi;
    (void)regs->rdx; // protocol
    const int AF_UNIX = 1;
    const int SOCK_STREAM = 1;
    const int SOCK_DGRAM = 2;
    int sock_type = type & 0xF;
    if (domain != AF_UNIX) return (uint64_t)-97; // EAFNOSUPPORT
    if (sock_type != SOCK_STREAM && sock_type != SOCK_DGRAM) return (uint64_t)-94; // ESOCKTNOSUPPORT

    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;

    open_files[fd] = &g_root_dir;
    fd_kind[fd] = FD_KIND_SOCKET;
    fd_flags[fd] = 0;
    fd_status[fd] = (uint32_t)type;
    fd_aux[fd] = nullptr;
    fd_pos[fd] = 0;

    unix_socket_state* us = &g_unix_sockets[fd];
    k_memset(us, 0, sizeof(*us));
    us->in_use = true;
    us->peer_fd = -1;
    return (uint64_t)fd;
}

uint64_t sys_bind(registers* regs) {
    int fd = (int)regs->rdi;
    linux_sockaddr_un* addr = (linux_sockaddr_un*)regs->rsi;
    uint64_t addrlen = regs->rdx;
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    if (!addr || !is_probably_user_ptr(addr) || addrlen < sizeof(uint16_t) + 2) return (uint64_t)-14;
    if (addr->sun_family != 1) return (uint64_t)-97;

    char norm[108];
    if (path_basename(addr->sun_path, norm, sizeof(norm)) != 0) return (uint64_t)-22;
    for (int i = 0; i < 100; ++i) {
        if (i == fd) continue;
        if (g_unix_sockets[i].in_use && g_unix_sockets[i].bound) {
            int same = 1;
            for (size_t j = 0; norm[j] || g_unix_sockets[i].path[j]; ++j) {
                if (norm[j] != g_unix_sockets[i].path[j]) { same = 0; break; }
            }
            if (same) return (uint64_t)-98; // EADDRINUSE
        }
    }

    unix_socket_state* us = &g_unix_sockets[fd];
    us->bound = true;
    k_memset(us->path, 0, sizeof(us->path));
    k_strcpy(us->path, norm);
    return 0;
}

uint64_t sys_listen(registers* regs) {
    int fd = (int)regs->rdi;
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    g_unix_sockets[fd].listening = true;
    return 0;
}

uint64_t sys_connect(registers* regs) {
    int fd = (int)regs->rdi;
    linux_sockaddr_un* addr = (linux_sockaddr_un*)regs->rsi;
    uint64_t addrlen = regs->rdx;
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    if (!addr || !is_probably_user_ptr(addr) || addrlen < sizeof(uint16_t) + 2) return (uint64_t)-14;
    if (addr->sun_family != 1) return (uint64_t)-97;

    char norm[108];
    if (path_basename(addr->sun_path, norm, sizeof(norm)) != 0) return (uint64_t)-22;

    int server_fd = -1;
    for (int i = 0; i < 100; ++i) {
        if (!g_unix_sockets[i].in_use || !g_unix_sockets[i].bound) continue;
        int same = 1;
        for (size_t j = 0; norm[j] || g_unix_sockets[i].path[j]; ++j) {
            if (norm[j] != g_unix_sockets[i].path[j]) { same = 0; break; }
        }
        if (same) { server_fd = i; break; }
    }
    if (server_fd < 0) return (uint64_t)-2;

    unix_socket_state* cli = &g_unix_sockets[fd];
    unix_socket_state* srv = &g_unix_sockets[server_fd];
    if (!srv->listening) return (uint64_t)-111; // ECONNREFUSED
    cli->connected = true;
    cli->peer_fd = server_fd;
    srv->connected = true;
    srv->peer_fd = fd;
    return 0;
}

uint64_t sys_accept(registers* regs) {
    int fd = (int)regs->rdi;
    (void)regs->rsi; // addr
    (void)regs->rdx; // addrlen
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    unix_socket_state* srv = &g_unix_sockets[fd];
    if (!srv->in_use || !srv->listening || !srv->connected || srv->peer_fd < 0) return (uint64_t)-11; // EAGAIN

    int nfd = get_free_fd();
    if (nfd < 0) return (uint64_t)-24;
    open_files[nfd] = &g_root_dir;
    fd_kind[nfd] = FD_KIND_SOCKET;
    fd_flags[nfd] = 0;
    fd_status[nfd] = fd_status[fd];
    fd_aux[nfd] = nullptr;
    fd_pos[nfd] = 0;

    unix_socket_state* acc = &g_unix_sockets[nfd];
    k_memset(acc, 0, sizeof(*acc));
    acc->in_use = true;
    acc->connected = true;
    acc->peer_fd = srv->peer_fd;

    unix_socket_state* cli = &g_unix_sockets[srv->peer_fd];
    cli->peer_fd = nfd;
    srv->connected = false;
    srv->peer_fd = -1;
    return (uint64_t)nfd;
}

uint64_t sys_sendmsg(registers* regs) {
    int fd = (int)regs->rdi;
    linux_msghdr* msg = (linux_msghdr*)regs->rsi;
    (void)regs->rdx; // flags
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    if (!msg || !is_probably_user_ptr(msg)) return (uint64_t)-14;

    linux_iovec* iov = (linux_iovec*)msg->msg_iov;
    if (msg->msg_iovlen > 64 || (msg->msg_iovlen && (!iov || !is_probably_user_ptr(iov)))) return (uint64_t)-14;
    unix_socket_state* us = &g_unix_sockets[fd];
    if (!us->connected || us->peer_fd < 0 || us->peer_fd >= 100) return (uint64_t)-107; // ENOTCONN
    unix_socket_state* peer = &g_unix_sockets[us->peer_fd];
    if (!peer->in_use || peer->q_count >= 16) return (uint64_t)-11; // EAGAIN

    unix_msg m{};
    for (uint64_t i = 0; i < msg->msg_iovlen; ++i) {
        if (!iov[i].iov_base || iov[i].iov_len == 0) continue;
        if (!is_probably_user_ptr(iov[i].iov_base)) return (uint64_t)-14;
        uint64_t chunk = iov[i].iov_len;
        if (chunk > sizeof(m.data) - m.len) chunk = sizeof(m.data) - m.len;
        if (!chunk) break;
        k_memcpy(m.data + m.len, iov[i].iov_base, chunk);
        m.len += (uint32_t)chunk;
        if (m.len >= sizeof(m.data)) break;
    }

    if (msg->msg_control && msg->msg_controllen >= sizeof(linux_cmsghdr) + sizeof(int)) {
        if (!is_probably_user_ptr(msg->msg_control)) return (uint64_t)-14;
        linux_cmsghdr* ch = (linux_cmsghdr*)msg->msg_control;
        if (ch->cmsg_level == 1 && ch->cmsg_type == 1) { // SOL_SOCKET / SCM_RIGHTS
            int* fds = (int*)((uint8_t*)msg->msg_control + sizeof(linux_cmsghdr));
            uint64_t payload = ch->cmsg_len > sizeof(linux_cmsghdr) ? (ch->cmsg_len - sizeof(linux_cmsghdr)) : 0;
            uint64_t cnt = payload / sizeof(int);
            if (cnt > 8) cnt = 8;
            for (uint64_t i = 0; i < cnt; ++i) {
                int passfd = fds[i];
                if (passfd >= 0 && passfd < 100 && open_files[passfd]) {
                    m.fds[m.fd_count++] = (int16_t)passfd;
                }
            }
        }
    }

    peer->queue[peer->q_tail] = m;
    peer->q_tail = (uint8_t)((peer->q_tail + 1) % 16);
    peer->q_count++;
    return m.len;
}

uint64_t sys_recvmsg(registers* regs) {
    int fd = (int)regs->rdi;
    linux_msghdr* msg = (linux_msghdr*)regs->rsi;
    (void)regs->rdx; // flags
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    if (!msg || !is_probably_user_ptr(msg)) return (uint64_t)-14;
    unix_socket_state* us = &g_unix_sockets[fd];
    if (!us->in_use || us->q_count == 0) return (uint64_t)-11; // EAGAIN

    linux_iovec* iov = (linux_iovec*)msg->msg_iov;
    if (msg->msg_iovlen > 64 || (msg->msg_iovlen && (!iov || !is_probably_user_ptr(iov)))) return (uint64_t)-14;

    unix_msg* m = &us->queue[us->q_head];
    uint64_t copied = 0;
    uint64_t src_off = 0;
    for (uint64_t i = 0; i < msg->msg_iovlen && src_off < m->len; ++i) {
        if (!iov[i].iov_base || iov[i].iov_len == 0) continue;
        if (!is_probably_user_ptr(iov[i].iov_base)) return (uint64_t)-14;
        uint64_t chunk = m->len - src_off;
        if (chunk > iov[i].iov_len) chunk = iov[i].iov_len;
        k_memcpy(iov[i].iov_base, m->data + src_off, chunk);
        src_off += chunk;
        copied += chunk;
    }

    if (msg->msg_control && msg->msg_controllen >= sizeof(linux_cmsghdr) + sizeof(int)) {
        if (!is_probably_user_ptr(msg->msg_control)) return (uint64_t)-14;
        linux_cmsghdr* ch = (linux_cmsghdr*)msg->msg_control;
        int* out_fds = (int*)((uint8_t*)msg->msg_control + sizeof(linux_cmsghdr));
        uint64_t max_out = (msg->msg_controllen - sizeof(linux_cmsghdr)) / sizeof(int);
        uint64_t out_cnt = m->fd_count < max_out ? m->fd_count : max_out;
        ch->cmsg_level = 1; // SOL_SOCKET
        ch->cmsg_type = 1;  // SCM_RIGHTS
        ch->cmsg_len = sizeof(linux_cmsghdr) + out_cnt * sizeof(int);
        for (uint64_t i = 0; i < out_cnt; ++i) {
            int src_fd = m->fds[i];
            if (src_fd >= 0 && src_fd < 100 && open_files[src_fd]) {
                int newfd = get_free_fd();
                if (newfd >= 0) {
                    open_files[newfd] = open_files[src_fd];
                    fd_kind[newfd] = fd_kind[src_fd];
                    fd_flags[newfd] = fd_flags[src_fd];
                    fd_status[newfd] = fd_status[src_fd];
                    fd_aux[newfd] = fd_aux[src_fd];
                    fd_pos[newfd] = fd_pos[src_fd];
                    out_fds[i] = newfd;
                } else {
                    out_fds[i] = -1;
                }
            } else {
                out_fds[i] = -1;
            }
        }
        msg->msg_controllen = ch->cmsg_len;
    } else {
        msg->msg_controllen = 0;
    }

    us->q_head = (uint8_t)((us->q_head + 1) % 16);
    us->q_count--;
    return copied;
}

uint64_t sys_getsockname(registers* regs) {
    int fd = (int)regs->rdi;
    linux_sockaddr_un* addr = (linux_sockaddr_un*)regs->rsi;
    uint32_t* addrlen = (uint32_t*)regs->rdx;
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    if (!addr || !addrlen || !is_probably_user_ptr(addr) || !is_probably_user_ptr(addrlen)) return (uint64_t)-14;

    unix_socket_state* us = &g_unix_sockets[fd];
    if (!us->bound) return (uint64_t)-107; // ENOTCONN-ish for unnamed
    uint32_t out_len = min_u64(*addrlen, (uint32_t)sizeof(linux_sockaddr_un));
    if (out_len < sizeof(uint16_t) + 1) return (uint64_t)-22;
    k_memset(addr, 0, out_len);
    addr->sun_family = 1; // AF_UNIX
    for (uint32_t i = 0; i + 1 < out_len - sizeof(uint16_t) && us->path[i]; ++i) {
        addr->sun_path[i] = us->path[i];
    }
    *addrlen = out_len;
    return 0;
}

uint64_t sys_getpeername(registers* regs) {
    int fd = (int)regs->rdi;
    linux_sockaddr_un* addr = (linux_sockaddr_un*)regs->rsi;
    uint32_t* addrlen = (uint32_t*)regs->rdx;
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    if (!addr || !addrlen || !is_probably_user_ptr(addr) || !is_probably_user_ptr(addrlen)) return (uint64_t)-14;

    unix_socket_state* us = &g_unix_sockets[fd];
    if (!us->connected || us->peer_fd < 0 || us->peer_fd >= 100) return (uint64_t)-107; // ENOTCONN
    unix_socket_state* peer = &g_unix_sockets[us->peer_fd];
    uint32_t out_len = min_u64(*addrlen, (uint32_t)sizeof(linux_sockaddr_un));
    if (out_len < sizeof(uint16_t) + 1) return (uint64_t)-22;
    k_memset(addr, 0, out_len);
    addr->sun_family = 1;
    for (uint32_t i = 0; i + 1 < out_len - sizeof(uint16_t) && peer->path[i]; ++i) {
        addr->sun_path[i] = peer->path[i];
    }
    *addrlen = out_len;
    return 0;
}

uint64_t sys_shutdown(registers* regs) {
    int fd = (int)regs->rdi;
    (void)regs->rsi; // how
    if (fd < 0 || fd >= 100 || !open_files[fd] || fd_kind[fd] != FD_KIND_SOCKET) return (uint64_t)-9;
    unix_socket_state* us = &g_unix_sockets[fd];
    us->connected = false;
    if (us->peer_fd >= 0 && us->peer_fd < 100) {
        unix_socket_state* peer = &g_unix_sockets[us->peer_fd];
        if (peer->in_use && peer->peer_fd == fd) {
            peer->connected = false;
            peer->peer_fd = -1;
        }
    }
    us->peer_fd = -1;
    return 0;
}

uint64_t sys_ftruncate(registers* regs) {
    int fd = (int)regs->rdi;
    uint64_t len = regs->rsi;
    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9;
    vfs_node* f = open_files[fd];
    if (f->is_directory) return (uint64_t)-21; // EISDIR
    if (!f->tar_data) {
        uint64_t cap = (len > 4096) ? len : 4096;
        f->tar_data = (uint8_t*)kmalloc(cap);
        if (!f->tar_data) return (uint64_t)-12;
        k_memset(f->tar_data, 0, cap);
        f->max_size = (uint32_t)cap;
    }
    if (len > f->max_size) {
        uint8_t* nb = (uint8_t*)kmalloc(len);
        if (!nb) return (uint64_t)-12;
        k_memset(nb, 0, len);
        if (f->size) k_memcpy(nb, f->tar_data, f->size);
        kfree(f->tar_data);
        f->tar_data = nb;
        f->max_size = (uint32_t)len;
    }
    if (len > f->size) {
        k_memset(f->tar_data + f->size, 0, len - f->size);
    }
    f->size = (uint32_t)len;
    if (fd_pos[fd] > len) fd_pos[fd] = len;
    return 0;
}

uint64_t sys_memfd_create(registers* regs) {
    const char* name = (const char*)regs->rdi;
    uint32_t flags = (uint32_t)regs->rsi;
    if (name && !is_probably_user_ptr(name)) return (uint64_t)-14;

    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;
    vfs_node* node = (vfs_node*)kmalloc(sizeof(vfs_node));
    if (!node) return (uint64_t)-12;
    k_memset(node, 0, sizeof(vfs_node));

    char tmp[64];
    const char* hex = "0123456789abcdef";
    const char* pfx = "memfd_";
    int t = 0;
    for (; pfx[t] && t < 63; ++t) tmp[t] = pfx[t];
    uint64_t id = g_memfd_counter++;
    for (int i = 0; i < 8 && t < 63; ++i) {
        tmp[t++] = hex[(id >> ((7 - i) * 4)) & 0xF];
    }
    tmp[t] = 0;
    k_strcpy(node->name, tmp);
    node->type = FS_FILE;
    node->source = FS_TAR;
    node->max_size = 4096;
    node->tar_data = (uint8_t*)kmalloc(node->max_size);
    if (!node->tar_data) {
        kfree(node);
        return (uint64_t)-12;
    }
    k_memset(node->tar_data, 0, node->max_size);
    node->next = vfs_root;
    vfs_root = node;

    open_files[fd] = node;
    fd_kind[fd] = FD_KIND_FILE;
    fd_flags[fd] = (flags & 0x80000U) ? 1U : 0U; // CLOEXEC approximation
    fd_status[fd] = (flags & 0x800U) ? 0x800U : 0U; // NONBLOCK approximation
    fd_aux[fd] = nullptr;
    fd_pos[fd] = 0;
    return (uint64_t)fd;
}

uint64_t sys_getdents64(registers* regs) {
    int fd = (int)regs->rdi;
    uint8_t* dirp = (uint8_t*)regs->rsi;
    uint64_t count = regs->rdx;
    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9;
    if (!dirp || !is_probably_user_ptr(dirp) || count < 24) return (uint64_t)-14;

    vfs_node* node = open_files[fd];
    if (!node->is_directory) return (uint64_t)-20; // ENOTDIR

    uint64_t written = 0;
    uint64_t idx = node->current_pos;
    vfs_node* it = vfs_root;
    while (it && idx) { it = it->next; idx--; }

    while (it) {
        uint64_t name_len = cstrlen(it->name);
        uint64_t reclen = 8 + 8 + 2 + 1 + name_len + 1;
        reclen = (reclen + 7) & ~7ULL;
        if (written + reclen > count) break;

        linux_dirent64* de = (linux_dirent64*)(dirp + written);
        de->d_ino = (it->addr ? it->addr : (uint64_t)(node->current_pos + 1));
        de->d_off = (int64_t)(node->current_pos + 1);
        de->d_reclen = (uint16_t)reclen;
        de->d_type = it->is_directory ? 4 : 8; // DT_DIR/DT_REG
        k_memset(de->d_name, 0, reclen - 19);
        k_memcpy(de->d_name, it->name, name_len);

        written += reclen;
        node->current_pos++;
        it = it->next;
    }
    return written;
}

uint64_t sys_getcwd(registers* regs) {
    char* buf = (char*)regs->rdi;
    uint64_t size = regs->rsi;
    if (!buf || size < 2) return (uint64_t)-22; // EINVAL
    if (!is_probably_user_ptr(buf)) return (uint64_t)-14;
    write_user_cstr(buf, "/", size);
    return (uint64_t)buf;
}

uint64_t sys_uname(registers* regs) {
    linux_utsname* u = (linux_utsname*)regs->rdi;
    if (!u) return (uint64_t)-14;
    if (!is_probably_user_ptr(u)) return (uint64_t)-14;

    write_user_cstr(u->sysname, "Linux", sizeof(u->sysname));
    write_user_cstr(u->nodename, "ams-1", sizeof(u->nodename));
    write_user_cstr(u->release, "0.1.0", sizeof(u->release));
    write_user_cstr(u->version, "#1 AMS", sizeof(u->version));
    write_user_cstr(u->machine, "x86_64", sizeof(u->machine));
    write_user_cstr(u->domainname, "localdomain", sizeof(u->domainname));
    return 0;
}

uint64_t sys_nanosleep(registers* regs) {
    (void)regs->rdi; // req
    (void)regs->rsi; // rem
    return 0;
}

uint64_t sys_rt_sigaction(registers* regs) {
    (void)regs->rdi; // signum
    (void)regs->rsi; // act
    (void)regs->rdx; // oldact
    (void)regs->r10; // sigsetsize
    return 0;
}

uint64_t sys_rt_sigprocmask(registers* regs) {
    (void)regs->rdi; // how
    (void)regs->rsi; // set
    (void)regs->rdx; // oldset
    (void)regs->r10; // sigsetsize
    return 0;
}

uint64_t sys_clock_gettime(registers* regs) {
    int clockid = (int)regs->rdi;
    linux_timespec* ts = (linux_timespec*)regs->rsi;
    if (!ts) return (uint64_t)-14;
    if (!is_probably_user_ptr(ts)) return (uint64_t)-14;

    if (clockid != 0 && clockid != 1) return (uint64_t)-22; // EINVAL
    uint64_t ticks = get_system_ticks();
    ts->tv_sec = (int64_t)(ticks / 100);
    ts->tv_nsec = (int64_t)((ticks % 100) * 10000000ULL);
    return 0;
}

uint64_t sys_gettimeofday(registers* regs) {
    linux_timeval* tv = (linux_timeval*)regs->rdi;
    (void)regs->rsi; // timezone obsolete
    if (!tv) return (uint64_t)-14;
    if (!is_probably_user_ptr(tv)) return (uint64_t)-14;
    uint64_t ticks = get_system_ticks();
    tv->tv_sec = (int64_t)(ticks / 100);
    tv->tv_usec = (int64_t)((ticks % 100) * 10000ULL);
    return 0;
}

uint64_t sys_getrandom(registers* regs) {
    uint8_t* buf = (uint8_t*)regs->rdi;
    uint64_t len = regs->rsi;
    (void)regs->rdx; // flags
    if (!buf) return (uint64_t)-14;
    if (!is_probably_user_ptr(buf)) return (uint64_t)-14;
    uint64_t seed = get_system_ticks() ^ (uint64_t)buf;
    for (uint64_t i = 0; i < len; ++i) {
        seed = seed * 1103515245ULL + 12345ULL;
        buf[i] = (uint8_t)(seed >> 24);
    }
    return len;
}

uint64_t sys_pread64(registers* regs) {
    int fd = (int)regs->rdi;
    uint8_t* buf = (uint8_t*)regs->rsi;
    size_t count = (size_t)regs->rdx;
    uint64_t off = regs->r10;
    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9;
    uint64_t saved = fd_pos[fd];
    fd_pos[fd] = off;
    registers r{};
    r.rdi = (uint64_t)fd;
    r.rsi = (uint64_t)buf;
    r.rdx = count;
    uint64_t rc = sys_read(&r);
    fd_pos[fd] = saved;
    return rc;
}

uint64_t sys_pwrite64(registers* regs) {
    int fd = (int)regs->rdi;
    uint8_t* buf = (uint8_t*)regs->rsi;
    size_t count = (size_t)regs->rdx;
    uint64_t off = regs->r10;
    if (fd < 0 || fd >= 100 || !open_files[fd]) return (uint64_t)-9;
    uint64_t saved = fd_pos[fd];
    fd_pos[fd] = off;
    registers r{};
    r.rdi = (uint64_t)fd;
    r.rsi = (uint64_t)buf;
    r.rdx = count;
    uint64_t rc = sys_write(&r);
    fd_pos[fd] = saved;
    return rc;
}

uint64_t sys_poll(registers* regs) {
    linux_pollfd* fds = (linux_pollfd*)regs->rdi;
    uint64_t nfds = regs->rsi;
    (void)regs->rdx; // timeout
    if (nfds > 4096) return (uint64_t)-22;
    if (nfds && (!fds || !is_probably_user_ptr(fds))) return (uint64_t)-14;
    uint64_t ready = 0;
    for (uint64_t i = 0; i < nfds; ++i) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        int fd = fds[i].fd;
        uint16_t req = (uint16_t)fds[i].events;
        uint16_t out = 0;
        const uint16_t POLLIN = 0x001;
        const uint16_t POLLOUT = 0x004;
        if (req == 0) req = POLLIN;
        if ((req & POLLIN) && fd_is_readable(fd)) out |= POLLIN;
        if ((req & POLLOUT) && fd_is_writable(fd)) out |= POLLOUT;
        fds[i].revents = (int16_t)out;
        if (out) ready++;
    }
    return ready;
}

uint64_t sys_ppoll(registers* regs) {
    registers r{};
    r.rdi = regs->rdi;
    r.rsi = regs->rsi;
    r.rdx = (regs->r10 == 0) ? (uint64_t)-1 : 0; // ignore timeout for now
    return sys_poll(&r);
}

uint64_t sys_exit_group(registers* regs) {
    return sys_exit(regs);
}

uint64_t sys_set_tid_address(registers* regs) {
    (void)regs->rdi;
    return 1;
}

uint64_t sys_set_robust_list(registers* regs) {
    (void)regs->rdi; // head
    (void)regs->rsi; // len
    return 0;
}

uint64_t sys_get_robust_list(registers* regs) {
    (void)regs->rdi; // pid
    void** head_ptr = (void**)regs->rsi;
    uint64_t* len_ptr = (uint64_t*)regs->rdx;
    if (!head_ptr || !len_ptr) return (uint64_t)-14;
    if (!is_probably_user_ptr(head_ptr) || !is_probably_user_ptr(len_ptr)) return (uint64_t)-14;
    *head_ptr = nullptr;
    *len_ptr = 0;
    return 0;
}

uint64_t sys_sigaltstack(registers* regs) {
    (void)regs->rdi; // ss
    (void)regs->rsi; // old_ss
    return 0;
}

uint64_t sys_futex(registers* regs) {
    int32_t* uaddr = (int32_t*)regs->rdi;
    int op = (int)regs->rsi;
    int val = (int)regs->rdx;
    (void)regs->r10; // timeout
    (void)regs->r8;  // uaddr2
    (void)regs->r9;  // val3
    if (!uaddr || !is_probably_user_ptr(uaddr)) return (uint64_t)-14;
    int cmd = op & 0x7f;
    if (cmd == 0) { // FUTEX_WAIT
        return (*uaddr == val) ? 0 : (uint64_t)-11; // EAGAIN if value changed
    }
    if (cmd == 1) { // FUTEX_WAKE
        return (uint64_t)val;
    }
    return (uint64_t)-38;
}

uint64_t sys_epoll_create1(registers* regs) {
    (void)regs->rdi; // flags currently ignored
    int slot = -1;
    for (int i = 0; i < 32; ++i) {
        if (!g_epolls[i].in_use) { slot = i; break; }
    }
    if (slot < 0) return (uint64_t)-24;
    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;

    g_epolls[slot].in_use = true;
    g_epolls[slot].watch_count = 0;

    open_files[fd] = &g_root_dir;
    fd_kind[fd] = FD_KIND_EPOLL;
    fd_flags[fd] = 0;
    fd_status[fd] = 0;
    fd_pos[fd] = 0;
    fd_aux[fd] = &g_epolls[slot];
    return (uint64_t)fd;
}

uint64_t sys_epoll_ctl(registers* regs) {
    int epfd = (int)regs->rdi;
    int op = (int)regs->rsi;
    int fd = (int)regs->rdx;
    linux_epoll_event* ev = (linux_epoll_event*)regs->r10;
    if (epfd < 0 || epfd >= 100 || fd < 0 || fd >= 100) return (uint64_t)-9;
    if (!open_files[epfd] || fd_kind[epfd] != FD_KIND_EPOLL || !fd_aux[epfd]) return (uint64_t)-9;
    if (!open_files[fd]) return (uint64_t)-9;
    epoll_state* ep = (epoll_state*)fd_aux[epfd];
    const int EPOLL_CTL_ADD = 1;
    const int EPOLL_CTL_DEL = 2;
    const int EPOLL_CTL_MOD = 3;

    int idx = -1;
    for (uint32_t i = 0; i < ep->watch_count; ++i) {
        if (ep->watches[i].fd == fd) { idx = (int)i; break; }
    }

    if (op == EPOLL_CTL_DEL) {
        if (idx < 0) return (uint64_t)-2;
        ep->watches[idx] = ep->watches[ep->watch_count - 1];
        ep->watch_count--;
        return 0;
    }
    if (!ev || !is_probably_user_ptr(ev)) return (uint64_t)-14;
    if (op == EPOLL_CTL_ADD) {
        if (idx >= 0) return (uint64_t)-17;
        if (ep->watch_count >= 64) return (uint64_t)-28;
        ep->watches[ep->watch_count].fd = fd;
        ep->watches[ep->watch_count].events = ev->events;
        ep->watches[ep->watch_count].data = ev->data;
        ep->watch_count++;
        return 0;
    }
    if (op == EPOLL_CTL_MOD) {
        if (idx < 0) return (uint64_t)-2;
        ep->watches[idx].events = ev->events;
        ep->watches[idx].data = ev->data;
        return 0;
    }
    return (uint64_t)-22;
}

uint64_t sys_epoll_wait(registers* regs) {
    int epfd = (int)regs->rdi;
    linux_epoll_event* events = (linux_epoll_event*)regs->rsi;
    int maxevents = (int)regs->rdx;
    (void)regs->r10; // timeout
    if (epfd < 0 || epfd >= 100 || !open_files[epfd] || fd_kind[epfd] != FD_KIND_EPOLL || !fd_aux[epfd]) return (uint64_t)-9;
    if (!events || maxevents <= 0) return (uint64_t)-22;
    if (!is_probably_user_ptr(events)) return (uint64_t)-14;
    epoll_state* ep = (epoll_state*)fd_aux[epfd];
    int emitted = 0;
    const uint32_t EPOLLIN = 0x001;
    const uint32_t EPOLLOUT = 0x004;
    for (uint32_t i = 0; i < ep->watch_count && emitted < maxevents; ++i) {
        epoll_watch* w = &ep->watches[i];
        uint32_t out = 0;
        if ((w->events & EPOLLIN) && fd_is_readable(w->fd)) out |= EPOLLIN;
        if ((w->events & EPOLLOUT) && fd_is_writable(w->fd)) out |= EPOLLOUT;
        if (!out && w->events == 0 && fd_is_readable(w->fd)) out |= EPOLLIN;
        if (out) {
            events[emitted].events = out;
            events[emitted].data = w->data;
            emitted++;
        }
    }
    return (uint64_t)emitted;
}

uint64_t sys_eventfd2(registers* regs) {
    uint32_t initv = (uint32_t)regs->rdi;
    uint32_t flags = (uint32_t)regs->rsi;
    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;
    eventfd_state* es = (eventfd_state*)kmalloc(sizeof(eventfd_state));
    if (!es) return (uint64_t)-12;
    es->value = initv;
    es->flags = flags;
    open_files[fd] = &g_root_dir;
    fd_kind[fd] = FD_KIND_EVENTFD;
    fd_flags[fd] = 0;
    fd_status[fd] = flags;
    fd_aux[fd] = es;
    fd_pos[fd] = 0;
    return (uint64_t)fd;
}

uint64_t sys_accept4(registers* regs) {
    registers r{};
    r.rdi = regs->rdi;
    r.rsi = regs->rsi;
    r.rdx = regs->rdx;
    return sys_accept(&r);
}

uint64_t sys_readlinkat(registers* regs) {
    registers r{};
    r.rdi = regs->rsi; // path
    r.rsi = regs->rdx; // buf
    r.rdx = regs->r10; // bufsiz
    return sys_readlink(&r);
}

uint64_t sys_statx(registers* regs) {
    (void)regs->rdi; // dirfd
    const char* path = (const char*)regs->rsi;
    (void)regs->rdx; // flags
    (void)regs->r10; // mask
    uint8_t* statxbuf = (uint8_t*)regs->r8;
    if (!path || !statxbuf) return (uint64_t)-14;
    char path_buf[256];
    if (!copy_user_path(path, path_buf, sizeof(path_buf))) return (uint64_t)-14;
    vfs_node* node = vfs_find(path_buf);
    if (!node) return (uint64_t)-2;
    k_memset(statxbuf, 0, 256);
    uint32_t mode = node->is_directory ? 0040755u : 0100644u;
    uint64_t size = node->size;
    k_memcpy(statxbuf + 28, &mode, sizeof(mode));
    k_memcpy(statxbuf + 40, &size, sizeof(size));
    return 0;
}

uint64_t sys_mremap(registers* regs) {
    (void)regs->rsi; (void)regs->rdx; (void)regs->r10; (void)regs->r8;
    return regs->rdi;
}

uint64_t sys_madvise(registers* regs) {
    (void)regs->rdi; (void)regs->rsi; (void)regs->rdx;
    return 0;
}

uint64_t sys_clone(registers* regs) {
    (void)regs->rdi; (void)regs->rsi; (void)regs->rdx; (void)regs->r10; (void)regs->r8;
    return (uint64_t)-38;
}

uint64_t sys_prlimit64(registers* regs) {
    (void)regs->rdi; // pid
    (void)regs->rsi; // resource
    void* new_rlim = (void*)regs->rdx;
    void* old_rlim = (void*)regs->r10;
    if (new_rlim) return (uint64_t)-1; // EPERM-ish for set
    if (old_rlim && is_probably_user_ptr(old_rlim)) {
        // Linux rlimit: rlim_cur + rlim_max
        uint64_t inf = ~0ULL;
        k_memcpy(old_rlim, &inf, sizeof(uint64_t));
        k_memcpy((uint8_t*)old_rlim + sizeof(uint64_t), &inf, sizeof(uint64_t));
    }
    return 0;
}

uint64_t sys_tgkill(registers* regs) {
    (void)regs->rdi; // tgid
    (void)regs->rsi; // tid
    (void)regs->rdx; // sig
    return 0;
}

uint64_t sys_wait4(registers* regs) {
    (void)regs->rdi; // pid
    (void)regs->rsi; // wstatus
    (void)regs->rdx; // options
    (void)regs->r10; // rusage
    // Na razie brak modelu zombie/reaping, zwracamy "brak dzieci".
    return (uint64_t)-10; // ECHILD
}

uint64_t sys_mmap(registers* regs) {
    /* rdi=addr, rsi=length, rdx=prot, r10=flags, r8=fd, r9=offset */
    uint64_t length = regs->rsi;
    uint64_t flags  = regs->r10;
    int      fd     = (int)regs->r8;
    uint64_t offset = regs->r9;

    length = (length + 4095) & ~4095ULL;
    if (length == 0) return (uint64_t)-22;

    /* MAP_SHARED with a DRM dumb-buffer mmap offset */
    if ((flags & 0x01) && fd >= 0 && fd < 100 && open_files[fd] &&
        fd_kind[fd] == FD_KIND_DRM && offset >= DUMB_MMAP_BASE) {
        uint64_t result = drm_virt_mmap_dumb(offset, length);
        if (result != (uint64_t)-1) return result;
    }

    /* MAP_SHARED with a memfd (or regular file): share the same physical pages
     * already backing the VFS node's tar_data buffer so multiple processes
     * read/write the same physical memory. */
    if ((flags & 0x01) && fd >= 0 && fd < 100 && open_files[fd] &&
        fd_kind[fd] == FD_KIND_FILE && open_files[fd]->tar_data) {
        vfs_node* f = open_files[fd];

        /* Ensure the file is large enough */
        uint64_t end = offset + length;
        if (end > f->max_size) {
            /* grow tar_data */
            uint8_t* nb = (uint8_t*)kmalloc(end);
            if (!nb) return (uint64_t)-12;
            k_memset(nb, 0, end);
            if (f->tar_data && f->size) k_memcpy(nb, f->tar_data, f->size);
            kfree(f->tar_data);
            f->tar_data = nb;
            f->max_size = (uint32_t)end;
        }
        if (end > f->size) f->size = (uint32_t)end;

        /* Map the file pages into the process address space.
         * tar_data lives in kernel heap — obtain physical addresses. */
        static uint64_t shm_va = 0x500000000ULL;
        uint64_t base = shm_va;
        uint64_t mapped = 0;
        while (mapped < length) {
            uint64_t kva = (uint64_t)(f->tar_data + offset + mapped);
            uint64_t phys = vmm_get_phys(kva);
            if (!phys) {
                /* fallback: allocate a new page and identity-copy */
                void* pg = pmm_alloc_frame();
                phys = (uint64_t)pg;
                k_memset((void*)(phys + 0xFFFF800000000000ULL), 0, 4096);
            }
            vmm_map_page_ex(current_task->cr3, base + mapped, phys, 0x7);
            mapped += 4096;
        }
        shm_va += length;
        return base;
    }

    /* Default: anonymous mapping */
    static uint64_t mmap_ptr = 0x400000000; // Startujemy wysoko
    uint64_t addr = mmap_ptr;

    for (uint64_t i = 0; i < length; i += 4096) {
        void* phys = pmm_alloc_frame();
        vmm_map_page_ex(current_task->cr3, addr + i, (uint64_t)phys, 0x7); // USER | WRITABLE | PRESENT
        k_memset((void*)((uint64_t)phys + 0xFFFF800000000000ULL), 0, 4096);
    }

    mmap_ptr += length;
    return addr;
}

uint64_t sys_mprotect(registers* regs) {
    (void)regs->rdi; (void)regs->rsi; (void)regs->rdx;
    return 0;
}

uint64_t sys_munmap(registers* regs) {
    uint64_t addr   = regs->rdi;
    uint64_t length = (regs->rsi + 4095) & ~4095ULL;
    if (addr == 0 || length == 0) return (uint64_t)-22;
    /* Unmap pages from the current process page table.
     * We don't reclaim physical frames here (no reverse mapping yet)
     * but we clear PTE entries so faults occur on stale access. */
    for (uint64_t off = 0; off < length; off += 4096) {
        uint64_t virt = addr + off;
        /* Map the virt to a null phys with no present bit to invalidate */
        vmm_map_page_ex(current_task->cr3, virt, 0, 0x0);
    }
    asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
    return 0;
}

uint64_t sys_brk(registers* regs) {
    uint64_t new_brk = regs->rdi;
    if (current_task->virt_memory_top == 0) current_task->virt_memory_top = 0x80000000;
    
    if (new_brk == 0) return current_task->virt_memory_top;

    // Mapowanie musi być ciągłe i bezpieczne
    if (new_brk > current_task->virt_memory_top) {
        for (uint64_t v = (current_task->virt_memory_top + 0xFFF) & ~0xFFF; v < new_brk; v += 4096) {
             void* p = pmm_alloc_frame();
             // Używamy vmm_map_page_ex, żeby nadać uprawnienia USER (0x7)
             vmm_map_page_ex(current_task->cr3, v, (uint64_t)p, 0x7);
        }
        current_task->virt_memory_top = new_brk;
    }
    return current_task->virt_memory_top;
}

uint64_t sys_exit(registers* regs) {
    int code = (int)regs->rdi;
    write_serial_string("[EXIT] Code: ");
    write_serial_dec(code);
    write_serial_string("\n");

    // Powrót do jądra MUSI użyć kernelowego CR3, nie CR3 ostatniego procesu user.
    // W przeciwnym razie kolejne alokacje/tablice stron mogą pagefaultować w ring0.
    asm volatile(
        "cli\n"
        "mov %0, %%cr3\n"
        "mov %1, %%rsp\n"
        "jmp *%2\n"
        : : "r"(g_kernel_cr3), "r"(kernel_task->kstack_top), "r"(kernel_task->rip) : "memory"
    );
    return 0; // Nigdy nie dojdzie
}

static uint64_t sys_ams_fb_blit(registers* regs) {
    const uint32_t* src = (const uint32_t*)regs->rdi;
    uint64_t w = regs->rsi;
    uint64_t h = regs->rdx;

    if (!src || !is_probably_user_ptr(src)) return (uint64_t)-14; // EFAULT
    if (!backbuffer || w == 0 || h == 0) return (uint64_t)-22;    // EINVAL
    if (w > fb_width || h > fb_height) return (uint64_t)-22;

    // Center Doom frame on current framebuffer.
    uint64_t off_x = (fb_width - w) / 2;
    uint64_t off_y = (fb_height - h) / 2;

    for (uint64_t y = 0; y < h; ++y) {
        uint32_t* dst_row = backbuffer + (off_y + y) * fb_width + off_x;
        uint64_t src_row_va = (uint64_t)(src + y * w);
        uint64_t bytes_left = w * sizeof(uint32_t);
        uint64_t copied = 0;

        while (bytes_left) {
            uint64_t cur_va = src_row_va + copied;
            uint64_t page_off = cur_va & 0xFFFULL;
            uint64_t chunk = 0x1000ULL - page_off;
            if (chunk > bytes_left) chunk = bytes_left;

            uint64_t phys = vmm_get_phys_ex(current_task->cr3, cur_va);
            if (!phys) return (uint64_t)-14; // EFAULT

            const void* src_ptr = (const void*)(phys + PHYS_OFFSET);
            k_memcpy((uint8_t*)dst_row + copied, src_ptr, chunk);
            copied += chunk;
            bytes_left -= chunk;
        }
    }
    graphics_flip();
    return 0;
}

extern "C" uint64_t sys_get_key();
static uint64_t sys_ams_get_key(registers* regs) {
    (void)regs;
    return sys_get_key();
}
extern "C" uint64_t sys_get_mouse_event();
static uint64_t sys_ams_get_mouse_event(registers* regs) {
    (void)regs;
    return sys_get_mouse_event();
}

static uint64_t sys_ams_get_fb_info(registers* regs) {
    uint32_t* out_w = (uint32_t*)regs->rdi;
    uint32_t* out_h = (uint32_t*)regs->rsi;
    if (!out_w || !out_h) return (uint64_t)-14; // EFAULT
    if (!is_probably_user_ptr(out_w) || !is_probably_user_ptr(out_h)) return (uint64_t)-14;
    *out_w = fb_width;
    *out_h = fb_height;
    return 0;
}

//sys_exec jest zdefiniowany w fs/elf.cpp, ale deklarujemy go tutaj, żeby móc go przypisać do syscall_table
extern "C" int sys_exec(const char* path, int argc, char** argv);

uint64_t sys_execve(registers* regs) {
    const char* path = (const char*)regs->rdi;
    char** argv = (char**)regs->rsi;
    (void)regs->rdx; // envp - na razie ignorujemy

    if (!path) return (uint64_t)-14; // EFAULT
    if (!is_probably_user_ptr(path)) return (uint64_t)-14;
    if (argv && !is_probably_user_ptr(argv)) return (uint64_t)-14;

    int argc = 0;
    if (argv) {
        while (argc < 64 && argv[argc]) {
            if (!is_probably_user_ptr(argv[argc])) return (uint64_t)-14;
            argc++;
        }
    }

    int rc = sys_exec(path, argc, argv ? argv : (char**)nullptr);
    return (uint64_t)rc;
}


/* ---- mlibc / wlroots additional syscall implementations ---- */

/* timerfd state — minimal stub */
struct timerfd_state {
    uint64_t interval_ns;
    uint64_t value;
};

static uint64_t sys_timerfd_create(registers* regs) {
    (void)regs->rdi; /* clockid */
    (void)regs->rsi; /* flags */
    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;
    timerfd_state* ts = (timerfd_state*)kmalloc(sizeof(timerfd_state));
    if (!ts) return (uint64_t)-12;
    k_memset(ts, 0, sizeof(*ts));
    open_files[fd] = &g_root_dir;
    fd_kind[fd]    = FD_KIND_TIMER;
    fd_flags[fd]   = 0;
    fd_status[fd]  = 0;
    fd_aux[fd]     = ts;
    fd_pos[fd]     = 0;
    return (uint64_t)fd;
}

static uint64_t sys_timerfd_settime(registers* regs) {
    int fd = (int)regs->rdi;
    if (fd < 0 || fd >= 100 || fd_kind[fd] != FD_KIND_TIMER) return (uint64_t)-9;
    timerfd_state* ts = (timerfd_state*)fd_aux[fd];
    if (!ts) return (uint64_t)-9;
    /* struct itimerspec at r10: [it_interval: {sec,nsec}, it_value: {sec,nsec}] */
    uint64_t* itimerspec = (uint64_t*)regs->r10;
    if (itimerspec && is_probably_user_ptr(itimerspec)) {
        ts->interval_ns = itimerspec[0] * 1000000000ULL + itimerspec[1];
        ts->value       = itimerspec[2] * 1000000000ULL + itimerspec[3];
    }
    return 0;
}

static uint64_t sys_timerfd_gettime(registers* regs) {
    int fd = (int)regs->rdi;
    if (fd < 0 || fd >= 100 || fd_kind[fd] != FD_KIND_TIMER) return (uint64_t)-9;
    /* Write zero itimerspec — timer never fires in this stub */
    uint64_t* out = (uint64_t*)regs->rsi;
    if (out && is_probably_user_ptr(out)) {
        k_memset(out, 0, 32); /* 4 × uint64_t */
    }
    return 0;
}

static uint64_t sys_signalfd4(registers* regs) {
    (void)regs;
    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;
    open_files[fd] = &g_root_dir;
    fd_kind[fd]    = FD_KIND_DEV_NULL;
    return (uint64_t)fd;
}

static uint64_t sys_inotify_init1(registers* regs) {
    (void)regs;
    int fd = get_free_fd();
    if (fd < 0) return (uint64_t)-24;
    open_files[fd] = &g_root_dir;
    fd_kind[fd]    = FD_KIND_DEV_NULL;
    return (uint64_t)fd;
}

static uint64_t sys_sched_getaffinity(registers* regs) {
    (void)regs->rdi; /* pid */
    uint64_t cpusetsize = regs->rsi;
    uint8_t* mask       = (uint8_t*)regs->rdx;
    if (!mask || !is_probably_user_ptr(mask)) return (uint64_t)-14;
    k_memset(mask, 0, cpusetsize);
    if (cpusetsize > 0) mask[0] = 1; /* single CPU */
    return 0;
}

static uint64_t sys_sched_yield(registers* regs) {
    (void)regs;
    return 0;
}

static uint64_t sys_getrlimit(registers* regs) {
    (void)regs->rdi; /* resource */
    uint64_t* rlim = (uint64_t*)regs->rsi;
    if (!rlim || !is_probably_user_ptr(rlim)) return (uint64_t)-14;
    rlim[0] = 0x7FFFFFFFFFFFFFFFULL; /* rlim_cur */
    rlim[1] = 0x7FFFFFFFFFFFFFFFULL; /* rlim_max */
    return 0;
}

static uint64_t sys_setrlimit(registers* regs) {
    (void)regs;
    return 0;
}

static uint64_t sys_prctl(registers* regs) {
    (void)regs;
    return 0;
}

static uint64_t sys_arch_prctl_impl(registers* regs) {
    int code = (int)regs->rdi;
    uint64_t addr = regs->rsi;
    /* ARCH_SET_FS = 0x1002 */
    if (code == 0x1002) {
        /* Write FS.base MSR so TLS works */
        uint32_t low  = (uint32_t)(addr & 0xFFFFFFFF);
        uint32_t high = (uint32_t)(addr >> 32);
        asm volatile("wrmsr" :: "c"((uint32_t)0xC0000100), "a"(low), "d"(high));
        return 0;
    }
    /* ARCH_GET_FS = 0x1003 */
    if (code == 0x1003) {
        uint64_t* out = (uint64_t*)addr;
        if (!out || !is_probably_user_ptr(out)) return (uint64_t)-14;
        uint32_t low, high;
        asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"((uint32_t)0xC0000100));
        *out = ((uint64_t)high << 32) | low;
        return 0;
    }
    return 0;
}

/* --- TABLICA DISPATCHERA --- */
syscall_fn syscall_table[SYSCALL_TABLE_SIZE];

void init_syscall_table() {
    init_root_dir_node();
    drm_virt_init();
    evdev_init();

    for (uint64_t i = 0; i < SYSCALL_TABLE_SIZE; ++i) {
        syscall_table[i] = sys_not_implemented;
    }

    syscall_table[SYS_READ]     = sys_read;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_OPEN]     = sys_open;
    syscall_table[SYS_POLL]     = sys_poll;
    syscall_table[SYS_PPOLL]    = sys_ppoll;
    syscall_table[SYS_LSEEK]    = sys_lseek;
    syscall_table[SYS_PREAD64]  = sys_pread64;
    syscall_table[SYS_PWRITE64] = sys_pwrite64;
    syscall_table[SYS_READV]    = sys_readv;
    syscall_table[SYS_WRITEV]   = sys_writev;
    syscall_table[SYS_OPENAT]   = sys_openat;
    syscall_table[SYS_ACCESS]   = sys_access;
    syscall_table[SYS_SOCKET]   = sys_socket;
    syscall_table[SYS_SHUTDOWN] = sys_shutdown;
    syscall_table[SYS_GETSOCKNAME] = sys_getsockname;
    syscall_table[SYS_GETPEERNAME] = sys_getpeername;
    syscall_table[SYS_BIND]     = sys_bind;
    syscall_table[SYS_LISTEN]   = sys_listen;
    syscall_table[SYS_ACCEPT]   = sys_accept;
    syscall_table[SYS_CONNECT]  = sys_connect;
    syscall_table[SYS_SENDMSG]  = sys_sendmsg;
    syscall_table[SYS_RECVMSG]  = sys_recvmsg;
    syscall_table[SYS_IOCTL]    = sys_ioctl;
    syscall_table[SYS_DUP]      = sys_dup;
    syscall_table[SYS_DUP2]     = sys_dup2;
    syscall_table[SYS_FCNTL]    = sys_fcntl;
    syscall_table[SYS_FTRUNCATE] = sys_ftruncate;
    syscall_table[SYS_PIPE2]    = sys_pipe2;
    syscall_table[SYS_GETDENTS64] = sys_getdents64;
    syscall_table[SYS_GETCWD]   = sys_getcwd;
    syscall_table[SYS_UNAME]    = sys_uname;
    syscall_table[SYS_NANOSLEEP]= sys_nanosleep;
    syscall_table[SYS_RT_SIGACTION] = sys_rt_sigaction;
    syscall_table[SYS_RT_SIGPROCMASK] = sys_rt_sigprocmask;
    syscall_table[SYS_CLOCK_GETTIME] = sys_clock_gettime;
    syscall_table[SYS_GETTIMEOFDAY] = sys_gettimeofday;
    syscall_table[SYS_FACCESSAT]= sys_faccessat;
    syscall_table[SYS_NEWFSTATAT] = sys_newfstatat;
    syscall_table[SYS_STAT]     = sys_stat;
    syscall_table[SYS_FSTAT]    = sys_fstat;
    syscall_table[SYS_READLINK] = sys_readlink;
    syscall_table[SYS_GETRANDOM]= sys_getrandom;
    syscall_table[SYS_CLOSE]    = [](registers* r) -> uint64_t { 
        int fd = (int)r->rdi;
        if(fd >= 3 && fd < 100) clear_fd_slot(fd);
        return 0; 
    };
    syscall_table[SYS_MMAP]     = sys_mmap;
    syscall_table[SYS_MPROTECT] = sys_mprotect;
    syscall_table[SYS_MUNMAP]   = sys_munmap;
    syscall_table[SYS_MREMAP]   = sys_mremap;
    syscall_table[SYS_MADVISE]  = sys_madvise;
    syscall_table[SYS_BRK]      = sys_brk;
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_EXIT_GROUP] = sys_exit_group;
    syscall_table[SYS_GETPID]   = [](registers* r) -> uint64_t { (void)r; return task_pid_or_default(); };
    syscall_table[SYS_ARCH_PRCTL] = [](registers* r) -> uint64_t {
        (void)r;
        // mlibc/TCC często sprawdza arch_prctl podczas startu.
        // Minimalny stub kompatybilności: sukces bez zmiany stanu.
        return 0;
    };

    //syscalle dla implementacji mlibc (TCC)
        syscall_table[SYS_EXECVE]   = sys_execve;
        syscall_table[SYS_FORK]     = sys_clone;
        syscall_table[SYS_CLONE]    = sys_clone;
        //set_tid_address, gettid, getppid, getuid, getgid, geteuid, getegid, etc. mogą być dodane później w razie potrzeby
        syscall_table[SYS_SET_TID_ADDRESS] = sys_set_tid_address;
        syscall_table[SYS_SET_ROBUST_LIST] = sys_set_robust_list;
        syscall_table[SYS_GET_ROBUST_LIST] = sys_get_robust_list;
        syscall_table[SYS_SIGALTSTACK] = sys_sigaltstack;
        syscall_table[SYS_FUTEX] = sys_futex;
        syscall_table[SYS_TGKILL] = sys_tgkill;
        syscall_table[SYS_GETTID] = [](registers* r) -> uint64_t { (void)r; return task_pid_or_default(); };
        syscall_table[SYS_GETPPID] = [](registers* r) -> uint64_t { (void)r; return kernel_task ? kernel_task->id : 0; };
        syscall_table[SYS_GETUID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETGID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETEUID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETEGID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_MEMFD_CREATE] = sys_memfd_create;
        syscall_table[SYS_EPOLL_CREATE1] = sys_epoll_create1;
        syscall_table[SYS_EPOLL_CTL] = sys_epoll_ctl;
        syscall_table[SYS_EPOLL_WAIT] = sys_epoll_wait;
        syscall_table[SYS_WAIT4] = sys_wait4;
        syscall_table[SYS_EVENTFD2] = sys_eventfd2;
        syscall_table[SYS_ACCEPT4] = sys_accept4;
        syscall_table[SYS_READLINKAT] = sys_readlinkat;
        syscall_table[SYS_STATX] = sys_statx;
        syscall_table[SYS_PRLIMIT64] = sys_prlimit64;
        syscall_table[SYS_AMS_FB_BLIT] = sys_ams_fb_blit;
        syscall_table[SYS_AMS_GET_KEY] = sys_ams_get_key;
        syscall_table[SYS_AMS_GET_FB_INFO] = sys_ams_get_fb_info;
        syscall_table[453] = sys_ams_get_mouse_event;

    /* mlibc / wlroots compatibility syscalls */
    syscall_table[158] = sys_arch_prctl_impl;         /* SYS_ARCH_PRCTL (override stub) */
    syscall_table[24]  = sys_sched_yield;             /* SYS_SCHED_YIELD */
    syscall_table[97]  = sys_getrlimit;               /* SYS_GETRLIMIT */
    syscall_table[160] = sys_setrlimit;               /* SYS_SETRLIMIT */
    syscall_table[157] = sys_prctl;                   /* SYS_PRCTL */
    syscall_table[204] = sys_sched_getaffinity;       /* SYS_SCHED_GETAFFINITY */
    syscall_table[283] = sys_timerfd_create;          /* SYS_TIMERFD_CREATE */
    syscall_table[286] = sys_timerfd_settime;         /* SYS_TIMERFD_SETTIME */
    syscall_table[287] = sys_timerfd_gettime;         /* SYS_TIMERFD_GETTIME */
    syscall_table[289] = sys_signalfd4;               /* SYS_SIGNALFD4 */
    syscall_table[294] = sys_inotify_init1;           /* SYS_INOTIFY_INIT1 */
}

// --- GŁÓWNY HANDLER ---
extern "C" uint64_t syscall_handler(registers* regs) {
    uint64_t id = regs->rax;
    
    if (id < SYSCALL_TABLE_SIZE && syscall_table[id]) {
        return syscall_table[id](regs);
    }
    
    return sys_not_implemented(regs);
}

extern "C" void syscall_init() {   
    // Najpierw podpinamy implementacje syscalli do tablicy dispatchera.
    init_syscall_table();

    // 1. Włącz SCE (System Call Extensions) - to już masz, ale upewnij się
    uint64_t efer = read_msr(0xC0000080);
    efer |= 1; 
    write_msr(0xC0000080, efer);

    // 2. Ustaw adres skoku (Twoja funkcja w ASM)
    write_msr(0xC0000082, (uint64_t)syscall_entry);

    // 3. Ustaw selektory segmentów (STAR)
    // Format: (UserBase << 48) | (KernelBase << 32)
    // KernelBase: 0x08 (CS), 0x10 (SS)
    // UserBase: 0x10 (bo Sysret doda +16 dla CS i +8 dla SS, co da 0x20 i 0x18)
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    write_msr(0xC0000081, star);

    // 4. Ustaw maskę flag (SFMASK)
    // Maskujemy: TF (0x100), IF (0x200), DF (0x400), IOPL (0x3000), AC (0x40000)
    // Najważniejsze: IF (0x200), aby syscall nie został przerwany przed swapgs
    write_msr(0xC0000084, 0x200); 

    // 5. Linux-style SWAPGS state:
    // IA32_GS_BASE (user) -> 0, IA32_KERNEL_GS_BASE -> &cpu_data.
    cpu_data.self = (uint64_t)&cpu_data;
    cpu_data.user_stack_scratch = 0;
    uint64_t boot_rsp = 0;
    asm volatile("mov %%rsp, %0" : "=r"(boot_rsp));
    cpu_data.kernel_stack = current_task ? current_task->kstack_top : boot_rsp;
    write_msr(0xC0000101, 0); // IA32_GS_BASE
    write_msr(0xC0000102, (uint64_t)&cpu_data); // IA32_KERNEL_GS_BASE
}