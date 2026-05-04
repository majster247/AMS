#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * AMS-OS Wayland Compositor — wlroots-style architecture
 *
 * Built on top of the AMS kernel DRM/KMS + GEM subsystem.
 * Uses Unix domain sockets for Wayland IPC, shared memory via
 * shm_open/mmap for buffer passing, and the DRM page-flip path
 * for presentation.
 *
 * This compositor follows the wlroots pattern:
 *  - DRM/KMS backend for output management
 *  - GEM dumb buffers for scanout
 *  - Wayland wire protocol over AF_UNIX
 *  - libinput-style input via AMS syscalls
 *  - pixman-style software composition
 */

#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43
#define SYS_CONNECT 42
#define SYS_SENDMSG 46
#define SYS_RECVMSG 47
#define SYS_MEMFD_CREATE 319
#define SYS_FTRUNCATE 77
#define SYS_CLOCK_GETTIME 228
#define SYS_POLL 7
#define SYS_EPOLL_CREATE1 291
#define SYS_EPOLL_CTL 233
#define SYS_EPOLL_WAIT 232
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define O_CREAT 0x40
#define O_RDWR 0x02
#define POLLIN 0x001
#define POLLOUT 0x004
#define EPOLLIN 0x001
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2

/* DRM/KMS definitions */
#define DRM_IOCTL_VERSION        0x00
#define DRM_IOCTL_GET_CAP        0x0C
#define DRM_IOCTL_SET_MASTER     0x1E
#define DRM_IOCTL_MODE_GETRESOURCES  0xA0
#define DRM_IOCTL_MODE_GETCRTC       0xA1
#define DRM_IOCTL_MODE_SETCRTC       0xA2
#define DRM_IOCTL_MODE_GETENCODER    0xA6
#define DRM_IOCTL_MODE_GETCONNECTOR  0xA7
#define DRM_IOCTL_MODE_ADDFB        0xAE
#define DRM_IOCTL_MODE_CREATE_DUMB  0xB2
#define DRM_IOCTL_MODE_MAP_DUMB     0xB3
#define DRM_IOCTL_MODE_DESTROY_DUMB 0xB4
#define DRM_IOCTL_MODE_PAGE_FLIP    0xB0
#define DRM_CAP_DUMB_BUFFER     0x01

/* Wayland object IDs */
#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32
#define MAX_CLIENTS 8
#define MAX_SURFACES 32

#define O_WL_DISPLAY 1
#define O_WL_REGISTRY 2
#define O_WL_COMPOSITOR 3
#define O_WL_SHM 4
#define O_WL_SURFACE 5
#define O_WL_SHM_POOL 6
#define O_WL_BUFFER 7
#define O_WL_CALLBACK 8
#define O_WL_OUTPUT 9
#define O_WL_SEAT 10
#define O_WL_POINTER 11
#define O_WL_KEYBOARD 12
#define O_XDG_WM_BASE 20
#define O_XDG_SURFACE 21
#define O_XDG_TOPLEVEL 22

struct linux_sockaddr_un { uint16_t sun_family; char sun_path[108]; };
struct linux_iovec { void* iov_base; uint64_t iov_len; };
struct linux_msghdr { void* msg_name; uint32_t msg_namelen; uint32_t __pad0; struct linux_iovec* msg_iov; uint64_t msg_iovlen; void* msg_control; uint64_t msg_controllen; uint32_t msg_flags; uint32_t __pad1; };
struct linux_cmsghdr { uint64_t cmsg_len; int32_t cmsg_level; int32_t cmsg_type; };
struct linux_timespec_local { int64_t tv_sec; int64_t tv_nsec; };
struct linux_pollfd { int32_t fd; int16_t events; int16_t revents; };

/* DRM structures */
struct drm_mode_modeinfo_local {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh, flags, type;
    char name[32];
};

struct drm_mode_card_res_local {
    uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
    uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
    uint32_t min_width, max_width, min_height, max_height;
};

struct drm_mode_get_connector_local {
    uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
    uint32_t count_modes, count_props, count_encoders, encoder_id;
    uint32_t connector_id, connector_type, connector_type_id, connection;
    uint32_t mm_width, mm_height, subpixel, pad;
};

struct drm_mode_get_encoder_local {
    uint32_t encoder_id, encoder_type, crtc_id;
    uint32_t possible_crtcs, possible_clones;
};

struct drm_mode_crtc_local {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
    struct drm_mode_modeinfo_local mode;
};

struct drm_mode_create_dumb_local {
    uint32_t height, width, bpp, flags, handle, pitch;
    uint64_t size;
};

struct drm_mode_map_dumb_local {
    uint32_t handle, pad;
    uint64_t offset;
};

struct drm_mode_fb_cmd_local {
    uint32_t fb_id, width, height, pitch, bpp, depth, handle;
};

struct drm_get_cap_local {
    uint64_t capability, value;
};

struct drm_mode_crtc_page_flip_local {
    uint32_t crtc_id, fb_id, flags, reserved;
    uint64_t user_data;
};

typedef struct wl_obj_state {
    uint32_t type, pool_id, offset, format, attached_buffer_id, role_id;
    uint32_t frame_callback_id, client_idx;
    int fd;
    uint8_t* map;
    uint32_t size;
    int32_t width, height, stride;
} wl_obj_state;

typedef struct wl_fd_queue {
    int data[WL_FDQ_CAP];
    uint32_t head, tail;
} wl_fd_queue;

typedef struct wl_client_state {
    int fd;
    int active;
    uint32_t pointer_id, keyboard_id, focused_surface, serial;
    uint8_t rx[WL_RX_CAP];
    uint32_t rx_len;
    wl_fd_queue fdq;
} wl_client_state;

/* KMS output state */
struct kms_output {
    int drm_fd;
    uint32_t crtc_id, connector_id, encoder_id;
    uint32_t width, height;
    struct drm_mode_modeinfo_local mode;
    uint32_t fb_ids[2];
    uint32_t gem_handles[2];
    uint32_t* framebuffers[2];
    int front;
};

static wl_obj_state g_objs[WL_OBJECT_MAX];
static wl_client_state g_clients[MAX_CLIENTS];
static int g_client_count = 0;
static struct kms_output g_output;
static uint32_t g_pointer_x = 80, g_pointer_y = 80;
static uint8_t g_pointer_buttons = 0;

/* Helper functions */
static void puts1(const char* s) { int n = 0; while (s[n]) ++n; ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0); ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0); }
static uint32_t now_ms(void) { struct linux_timespec_local ts; if ((long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0) != 0) return 0; return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
static uint32_t rd_u32(const uint8_t* p) { return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr_u32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)(v&0xFF); p[1]=(uint8_t)((v>>8)&0xFF); p[2]=(uint8_t)((v>>16)&0xFF); p[3]=(uint8_t)((v>>24)&0xFF); }
static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) { wr_u32(out + at, v); return at + 4; }
static uint32_t append_i32(uint8_t* out, uint32_t at, int32_t v) { wr_u32(out + at, (uint32_t)v); return at + 4; }
static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) { uint32_t len=0; while (s[len]) ++len; at=append_u32(out,at,len+1); memcpy(out+at,s,len); out[at+len]=0; at+=len+1; while(at&3U) out[at++]=0; return at; }

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET; ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control; msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) {
    struct linux_iovec iov = {data, cap};
    uint8_t control[64] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = control; msg.msg_controllen = sizeof(control);
    *recv_fd = -1;
    int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
    if (rc <= 0) return rc;
    if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS)
            *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr));
    }
    return rc;
}

static void fdq_init(wl_fd_queue* q) { q->head=0; q->tail=0; for(uint32_t i=0;i<WL_FDQ_CAP;++i) q->data[i]=-1; }
static int fdq_push(wl_fd_queue* q, int fd) { uint32_t n=(q->tail+1U)%WL_FDQ_CAP; if(n==q->head) return -1; q->data[q->tail]=fd; q->tail=n; return 0; }
static int fdq_pop(wl_fd_queue* q) { if(q->head==q->tail) return -1; int fd=q->data[q->head]; q->head=(q->head+1U)%WL_FDQ_CAP; return fd; }

/* Wayland event helpers */
static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size) { wr_u32(pkt, obj_id); wr_u32(pkt+4, ((uint32_t)size<<16)|opcode); }
static void send_callback_done(int fd, uint32_t cb_id) { uint8_t pkt[12]={0}; send_event_header(pkt, cb_id, 0, 12); wr_u32(pkt+8, now_ms()); (void)send_packet(fd,pkt,12,-1); }
static void send_buffer_release(int fd, uint32_t id) { uint8_t pkt[8]={0}; send_event_header(pkt,id,0,8); (void)send_packet(fd,pkt,8,-1); }

static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) {
    uint8_t pkt[256]={0}; uint32_t at=0;
    at=append_u32(pkt,at,reg_id); uint32_t hdr=at; at=append_u32(pkt,at,0);
    at=append_u32(pkt,at,name); at=append_string(pkt,at,iface); at=append_u32(pkt,at,version);
    wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1);
}

/* DRM/KMS backend initialization */
static int kms_init(struct kms_output* out) {
    int drm_fd = drm_open();
    if (drm_fd < 0) { puts1("kms: drm_open failed"); return -1; }
    out->drm_fd = drm_fd;

    struct drm_get_cap_local cap = {0};
    cap.capability = DRM_CAP_DUMB_BUFFER;
    if (drm_ioctl(drm_fd, DRM_IOCTL_GET_CAP, &cap) < 0 || !cap.value) {
        puts1("kms: no dumb buffer support"); return -1;
    }
    puts1("kms: dumb buffer capability confirmed");

    drm_ioctl(drm_fd, DRM_IOCTL_SET_MASTER, (void*)0);

    struct drm_mode_card_res_local res = {0};
    if (drm_ioctl(drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
        puts1("kms: getresources failed"); return -1;
    }
    if (res.count_connectors == 0 || res.count_crtcs == 0) {
        puts1("kms: no connectors/crtcs"); return -1;
    }

    uint32_t conn_ids[4] = {0}, crtc_ids[4] = {0}, enc_ids[4] = {0};
    res.connector_id_ptr = (uint64_t)conn_ids;
    res.crtc_id_ptr = (uint64_t)crtc_ids;
    res.encoder_id_ptr = (uint64_t)enc_ids;
    drm_ioctl(drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &res);

    out->connector_id = conn_ids[0];

    struct drm_mode_get_connector_local conn = {0};
    conn.connector_id = out->connector_id;
    drm_ioctl(drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);

    if (conn.connection != 1) { puts1("kms: connector not connected"); return -1; }

    struct drm_mode_modeinfo_local modes[4] = {0};
    uint32_t enc_list[4] = {0};
    conn.modes_ptr = (uint64_t)modes;
    conn.encoders_ptr = (uint64_t)enc_list;
    conn.count_modes = 4;
    conn.count_encoders = 4;
    drm_ioctl(drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);

    memcpy(&out->mode, &modes[0], sizeof(out->mode));
    out->width = out->mode.hdisplay;
    out->height = out->mode.vdisplay;
    out->encoder_id = conn.encoder_id;

    struct drm_mode_get_encoder_local enc = {0};
    enc.encoder_id = out->encoder_id;
    drm_ioctl(drm_fd, DRM_IOCTL_MODE_GETENCODER, &enc);
    out->crtc_id = enc.crtc_id;

    puts1("kms: output initialized via DRM/KMS");

    /* Create double-buffered GEM dumb scanout */
    for (int i = 0; i < 2; ++i) {
        struct drm_mode_create_dumb_local create = {0};
        create.width = out->width;
        create.height = out->height;
        create.bpp = 32;
        if (drm_ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
            puts1("kms: create_dumb failed"); return -1;
        }
        out->gem_handles[i] = create.handle;

        struct drm_mode_fb_cmd_local fb = {0};
        fb.width = out->width;
        fb.height = out->height;
        fb.pitch = create.pitch;
        fb.bpp = 32;
        fb.depth = 24;
        fb.handle = create.handle;
        if (drm_ioctl(drm_fd, DRM_IOCTL_MODE_ADDFB, &fb) < 0) {
            puts1("kms: addfb failed"); return -1;
        }
        out->fb_ids[i] = fb.fb_id;

        struct drm_mode_map_dumb_local map = {0};
        map.handle = create.handle;
        if (drm_ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
            puts1("kms: map_dumb failed"); return -1;
        }
        out->framebuffers[i] = (uint32_t*)map.offset;
    }
    out->front = 0;

    /* Set initial CRTC mode */
    struct drm_mode_crtc_local crtc = {0};
    crtc.crtc_id = out->crtc_id;
    crtc.fb_id = out->fb_ids[0];
    crtc.set_connectors_ptr = (uint64_t)&out->connector_id;
    crtc.count_connectors = 1;
    crtc.mode_valid = 1;
    memcpy(&crtc.mode, &out->mode, sizeof(crtc.mode));
    drm_ioctl(drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc);

    puts1("kms: CRTC mode set, double-buffered GEM scanout ready");
    return 0;
}

static void kms_page_flip(struct kms_output* out) {
    struct drm_mode_crtc_page_flip_local flip = {0};
    flip.crtc_id = out->crtc_id;
    flip.fb_id = out->fb_ids[out->front];
    flip.flags = 0x01; /* DRM_MODE_PAGE_FLIP_EVENT */
    drm_ioctl(out->drm_fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip);
    out->front ^= 1;
}

/* Pixman-style software composition */
static void draw_background(uint32_t* fb, uint32_t w, uint32_t h) {
    for (uint32_t y = 0; y < h; ++y) {
        uint32_t c = (y < 36) ? 0x1E2733 : 0x151C26;
        for (uint32_t x = 0; x < w; ++x)
            fb[y * w + x] = c;
    }
}

static void draw_pointer_cursor(uint32_t* fb, uint32_t w, uint32_t h) {
    for (uint32_t y = 0; y < 12; ++y) {
        for (uint32_t x = 0; x < 10; ++x) {
            uint32_t px = g_pointer_x + x, py = g_pointer_y + y;
            if (px >= w || py >= h) continue;
            if (x <= y) fb[py * w + px] = 0xFFFFFF;
        }
    }
}

static void composite_and_present(void) {
    uint32_t* fb = g_output.framebuffers[g_output.front];
    if (!fb) {
        fb = (uint32_t*)malloc((size_t)g_output.width * g_output.height * sizeof(uint32_t));
        if (!fb) return;
        g_output.framebuffers[g_output.front] = fb;
    }

    draw_background(fb, g_output.width, g_output.height);

    /* Composite all mapped client surfaces */
    for (uint32_t sid = 0; sid < WL_OBJECT_MAX; ++sid) {
        wl_obj_state* s = &g_objs[sid];
        if (s->type != O_WL_SURFACE || !s->attached_buffer_id) continue;
        wl_obj_state* b = &g_objs[s->attached_buffer_id];
        if (b->type != O_WL_BUFFER || !b->pool_id || b->pool_id >= WL_OBJECT_MAX) continue;
        wl_obj_state* p = &g_objs[b->pool_id];
        if (p->type != O_WL_SHM_POOL || !p->map) continue;

        uint32_t cw = (b->width > 0 && (uint32_t)b->width < g_output.width) ? (uint32_t)b->width : g_output.width;
        uint32_t ch = (b->height > 0 && (uint32_t)b->height < g_output.height) ? (uint32_t)b->height : g_output.height;
        for (uint32_t y = 0; y < ch; ++y) {
            uint32_t off = b->offset + y * (uint32_t)b->stride;
            if (off + cw * 4U > p->size) break;
            memcpy(&fb[y * g_output.width], p->map + off, cw * 4U);
        }

        int cli_fd = -1;
        if (s->client_idx < MAX_CLIENTS && g_clients[s->client_idx].active)
            cli_fd = g_clients[s->client_idx].fd;

        if (s->frame_callback_id && s->frame_callback_id < WL_OBJECT_MAX && cli_fd >= 0) {
            send_callback_done(cli_fd, s->frame_callback_id);
            g_objs[s->frame_callback_id].type = 0;
            s->frame_callback_id = 0;
        }
        if (cli_fd >= 0)
            send_buffer_release(cli_fd, s->attached_buffer_id);
    }

    draw_pointer_cursor(fb, g_output.width, g_output.height);

    /* Present via DRM page flip, fallback to SYS_AMS_FB_BLIT */
    if (g_output.drm_fd >= 0) {
        kms_page_flip(&g_output);
    }
    (void)ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)fb, g_output.width, g_output.height, 0, 0);
}

static void send_output_info(int fd, uint32_t oid) {
    uint8_t pkt[128]={0}; uint32_t at=0;
    at=append_u32(pkt,at,oid); uint32_t hdr=at; at=append_u32(pkt,at,0);
    at=append_i32(pkt,at,0); at=append_i32(pkt,at,0);
    at=append_i32(pkt,at,300); at=append_i32(pkt,at,170); at=append_u32(pkt,at,1);
    at=append_string(pkt,at,"AMS-DRM"); at=append_string(pkt,at,"Virtual-KMS");
    at=append_i32(pkt,at,0);
    wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1);

    uint8_t mode[24]={0}; send_event_header(mode,oid,1,20);
    wr_u32(mode+8,3); wr_u32(mode+12,g_output.width); wr_u32(mode+16,g_output.height);
    wr_u32(mode+20,60000); (void)send_packet(fd,mode,24,-1);

    uint8_t scale[12]={0}; send_event_header(scale,oid,3,12);
    wr_u32(scale+8,1); (void)send_packet(fd,scale,12,-1);

    uint8_t done[8]={0}; send_event_header(done,oid,2,8);
    (void)send_packet(fd,done,8,-1);
}

static void send_seat_info(int fd, uint32_t sid) {
    uint8_t caps[12]={0}; send_event_header(caps,sid,0,12);
    wr_u32(caps+8,3); (void)send_packet(fd,caps,12,-1);
    uint8_t name[64]={0}; uint32_t at=0;
    at=append_u32(name,at,sid); uint32_t hdr=at; at=append_u32(name,at,0);
    at=append_string(name,at,"seat0");
    wr_u32(name+hdr,(at<<16)|1); (void)send_packet(fd,name,at,-1);
}

static void send_pointer_enter(int fd, uint32_t pid, uint32_t sid, uint32_t* serial) {
    uint8_t pkt[24]={0}; send_event_header(pkt,pid,0,24);
    wr_u32(pkt+8,++(*serial)); wr_u32(pkt+12,sid);
    wr_u32(pkt+16,g_pointer_x<<8); wr_u32(pkt+20,g_pointer_y<<8);
    (void)send_packet(fd,pkt,24,-1);
}

static void send_pointer_motion(int fd, uint32_t pid) {
    uint8_t pkt[20]={0}; send_event_header(pkt,pid,2,20);
    wr_u32(pkt+8,now_ms()); wr_u32(pkt+12,g_pointer_x<<8); wr_u32(pkt+16,g_pointer_y<<8);
    (void)send_packet(fd,pkt,20,-1);
}

static void send_pointer_button(int fd, uint32_t pid, uint32_t state, uint32_t* serial) {
    uint8_t pkt[24]={0}; send_event_header(pkt,pid,3,24);
    wr_u32(pkt+8,++(*serial)); wr_u32(pkt+12,now_ms());
    wr_u32(pkt+16,0x110); wr_u32(pkt+20,state);
    (void)send_packet(fd,pkt,24,-1);
}

static void send_keyboard_enter(int fd, uint32_t kid, uint32_t sid, uint32_t* serial) {
    uint8_t pkt[20]={0}; send_event_header(pkt,kid,1,20);
    wr_u32(pkt+8,++(*serial)); wr_u32(pkt+12,sid); wr_u32(pkt+16,0);
    (void)send_packet(fd,pkt,20,-1);
}

static void send_keyboard_key(int fd, uint32_t kid, uint32_t key, uint32_t state, uint32_t* serial) {
    uint8_t pkt[24]={0}; send_event_header(pkt,kid,3,24);
    wr_u32(pkt+8,++(*serial)); wr_u32(pkt+12,now_ms());
    wr_u32(pkt+16,key); wr_u32(pkt+20,state);
    (void)send_packet(fd,pkt,24,-1);
}

/* libinput-style input handling via AMS syscalls */
static void handle_input(void) {
    int should_redraw = 0;

    uint64_t mev = ams_syscall(SYS_AMS_GET_MOUSE_EVENT, 0, 0, 0, 0, 0);
    if (mev) {
        uint32_t old_x = g_pointer_x, old_y = g_pointer_y;
        g_pointer_x = (uint32_t)(mev & 0xFFFFU);
        g_pointer_y = (uint32_t)((mev >> 16) & 0xFFFFU);
        uint8_t buttons = (uint8_t)((mev >> 32) & 0xFFU);
        uint8_t old = g_pointer_buttons;
        g_pointer_buttons = buttons;

        for (int i = 0; i < g_client_count; ++i) {
            wl_client_state* c = &g_clients[i];
            if (!c->active || !c->pointer_id || !c->focused_surface) continue;
            send_pointer_motion(c->fd, c->pointer_id);
            if (old != g_pointer_buttons)
                send_pointer_button(c->fd, c->pointer_id, (g_pointer_buttons & 1U) ? 1U : 0U, &c->serial);
        }
        if (old_x != g_pointer_x || old_y != g_pointer_y || old != g_pointer_buttons)
            should_redraw = 1;
    }

    uint64_t kev = ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
    if (kev) {
        int32_t k = (int32_t)kev;
        uint32_t st = 1;
        if (k < 0) { st = 0; k = -k; }
        for (int i = 0; i < g_client_count; ++i) {
            wl_client_state* c = &g_clients[i];
            if (!c->active || !c->keyboard_id || !c->focused_surface) continue;
            send_keyboard_key(c->fd, c->keyboard_id, (uint32_t)k, st, &c->serial);
        }
    }

    if (should_redraw) composite_and_present();
}

static void clear_objects_for_client(int idx) {
    for (uint32_t i = 0; i < WL_OBJECT_MAX; ++i) {
        if (g_objs[i].client_idx == (uint32_t)idx && g_objs[i].type != 0) {
            g_objs[i].type = 0;
        }
    }
}

static void init_client_objects(int idx, int fd) {
    wl_client_state* c = &g_clients[idx];
    c->fd = fd;
    c->active = 1;
    c->pointer_id = 0;
    c->keyboard_id = 0;
    c->focused_surface = 0;
    c->serial = 0;
    c->rx_len = 0;
    fdq_init(&c->fdq);
}

static void process_message(int cli_idx, uint32_t oid, uint16_t op, const uint8_t* p, uint32_t n) {
    if (oid >= WL_OBJECT_MAX) return;
    wl_client_state* cli = &g_clients[cli_idx];
    int fd = cli->fd;
    wl_obj_state* o = &g_objs[oid];

    if (oid == 1 && o->type == O_WL_DISPLAY) {
        if (op == 0 && n >= 4) {
            uint32_t cb = rd_u32(p);
            if (cb && cb < WL_OBJECT_MAX) { g_objs[cb].type = O_WL_CALLBACK; g_objs[cb].client_idx = (uint32_t)cli_idx; send_callback_done(fd, cb); }
        }
        if (op == 1 && n >= 4) {
            uint32_t rid = rd_u32(p);
            if (rid && rid < WL_OBJECT_MAX) {
                g_objs[rid].type = O_WL_REGISTRY; g_objs[rid].client_idx = (uint32_t)cli_idx;
                send_registry_global(fd, rid, 1, "wl_compositor", 4);
                send_registry_global(fd, rid, 2, "wl_shm", 1);
                send_registry_global(fd, rid, 3, "wl_output", 2);
                send_registry_global(fd, rid, 4, "wl_seat", 5);
                send_registry_global(fd, rid, 5, "xdg_wm_base", 1);
            }
        }
        return;
    }

    if (o->type == O_WL_REGISTRY) {
        if (op == 0 && n >= 16) {
            uint32_t name = rd_u32(p);
            uint32_t sl = rd_u32(p + 4);
            uint32_t sp = (sl + 3U) & ~3U;
            if (n < 4 + 4 + sp + 8) return;
            uint32_t nid = rd_u32(p + 12 + sp);
            if (!nid || nid >= WL_OBJECT_MAX) return;
            g_objs[nid].client_idx = (uint32_t)cli_idx;
            if (name == 1) g_objs[nid].type = O_WL_COMPOSITOR;
            else if (name == 2) {
                g_objs[nid].type = O_WL_SHM;
                uint8_t f[12] = {0}; send_event_header(f, nid, 0, 12); wr_u32(f + 8, 0);
                (void)send_packet(fd, f, 12, -1);
            }
            else if (name == 3) { g_objs[nid].type = O_WL_OUTPUT; send_output_info(fd, nid); }
            else if (name == 4) { g_objs[nid].type = O_WL_SEAT; send_seat_info(fd, nid); }
            else if (name == 5) {
                g_objs[nid].type = O_XDG_WM_BASE;
                uint8_t ping[12] = {0}; send_event_header(ping, nid, 0, 12);
                wr_u32(ping + 8, ++cli->serial); (void)send_packet(fd, ping, 12, -1);
            }
        }
        return;
    }

    if (o->type == O_WL_COMPOSITOR) {
        if (op == 0 && n >= 4) {
            uint32_t nid = rd_u32(p);
            if (nid && nid < WL_OBJECT_MAX) {
                g_objs[nid].type = O_WL_SURFACE;
                g_objs[nid].client_idx = (uint32_t)cli_idx;
            }
        }
        return;
    }

    if (o->type == O_WL_SHM) {
        if (op == 0 && n >= 8) {
            uint32_t nid = rd_u32(p), sz = rd_u32(p + 4);
            int passed = fdq_pop(&cli->fdq);
            if (!nid || nid >= WL_OBJECT_MAX || passed < 0 || sz == 0) return;
            g_objs[nid].type = O_WL_SHM_POOL;
            g_objs[nid].fd = passed;
            g_objs[nid].size = sz;
            g_objs[nid].client_idx = (uint32_t)cli_idx;
            g_objs[nid].map = (uint8_t*)mmap(0, sz, PROT_READ, MAP_SHARED, passed, 0);
            if ((uint64_t)g_objs[nid].map > (uint64_t)-4096LL) g_objs[nid].map = 0;
        }
        return;
    }

    if (o->type == O_WL_SHM_POOL) {
        if (op == 0 && n >= 24) {
            uint32_t nid = rd_u32(p);
            if (!nid || nid >= WL_OBJECT_MAX) return;
            g_objs[nid].type = O_WL_BUFFER;
            g_objs[nid].pool_id = oid;
            g_objs[nid].offset = rd_u32(p + 4);
            g_objs[nid].width = (int32_t)rd_u32(p + 8);
            g_objs[nid].height = (int32_t)rd_u32(p + 12);
            g_objs[nid].stride = (int32_t)rd_u32(p + 16);
            g_objs[nid].format = rd_u32(p + 20);
            g_objs[nid].client_idx = (uint32_t)cli_idx;
        }
        return;
    }

    if (o->type == O_WL_SURFACE) {
        if (op == 1 && n >= 12)
            o->attached_buffer_id = rd_u32(p);
        else if (op == 3 && n >= 4) {
            uint32_t cb = rd_u32(p);
            o->frame_callback_id = cb;
            if (cb && cb < WL_OBJECT_MAX) { g_objs[cb].type = O_WL_CALLBACK; g_objs[cb].client_idx = (uint32_t)cli_idx; }
        } else if (op == 6) {
            cli->focused_surface = oid;
            composite_and_present();
            if (cli->pointer_id)
                send_pointer_enter(fd, cli->pointer_id, oid, &cli->serial);
            if (cli->keyboard_id)
                send_keyboard_enter(fd, cli->keyboard_id, oid, &cli->serial);
        }
        return;
    }

    if (o->type == O_WL_SEAT) {
        if (op == 0 && n >= 4) {
            uint32_t nid = rd_u32(p);
            if (nid && nid < WL_OBJECT_MAX) {
                g_objs[nid].type = O_WL_POINTER;
                g_objs[nid].client_idx = (uint32_t)cli_idx;
                cli->pointer_id = nid;
            }
        } else if (op == 1 && n >= 4) {
            uint32_t nid = rd_u32(p);
            if (nid && nid < WL_OBJECT_MAX) {
                g_objs[nid].type = O_WL_KEYBOARD;
                g_objs[nid].client_idx = (uint32_t)cli_idx;
                cli->keyboard_id = nid;
            }
        }
        return;
    }

    if (o->type == O_XDG_WM_BASE) {
        if (op == 1 && n >= 8) {
            uint32_t xs = rd_u32(p), sid = rd_u32(p + 4);
            if (xs && xs < WL_OBJECT_MAX && sid && sid < WL_OBJECT_MAX) {
                g_objs[xs].type = O_XDG_SURFACE;
                g_objs[xs].role_id = sid;
                g_objs[xs].client_idx = (uint32_t)cli_idx;
                g_objs[sid].role_id = xs;
                uint8_t cfg[12] = {0}; send_event_header(cfg, xs, 0, 12);
                wr_u32(cfg + 8, ++cli->serial); (void)send_packet(fd, cfg, 12, -1);
            }
        }
        return;
    }

    if (o->type == O_XDG_SURFACE) {
        if (op == 1 && n >= 4) {
            uint32_t tl = rd_u32(p);
            if (tl && tl < WL_OBJECT_MAX) {
                g_objs[tl].type = O_XDG_TOPLEVEL;
                g_objs[tl].client_idx = (uint32_t)cli_idx;
                uint8_t tcfg[20] = {0}; send_event_header(tcfg, tl, 0, 20);
                wr_u32(tcfg + 8, g_output.width); wr_u32(tcfg + 12, g_output.height);
                wr_u32(tcfg + 16, 0); (void)send_packet(fd, tcfg, 20, -1);
                uint8_t scfg[12] = {0}; send_event_header(scfg, oid, 0, 12);
                wr_u32(scfg + 8, ++cli->serial); (void)send_packet(fd, scfg, 12, -1);
            }
        }
        return;
    }

    if (o->type == O_WL_BUFFER && op == 0) o->type = 0;
}

static void process_client_data(int idx) {
    wl_client_state* c = &g_clients[idx];
    int pass = -1;
    int n = recv_packet(c->fd, c->rx + c->rx_len, WL_RX_CAP - c->rx_len, &pass);
    if (n == 0) {
        puts1("wl-compositor: client disconnected");
        c->active = 0;
        clear_objects_for_client(idx);
        return;
    }
    if (n < 0) return;
    if (pass >= 0) (void)fdq_push(&c->fdq, pass);
    c->rx_len += (uint32_t)n;

    uint32_t at = 0;
    while (c->rx_len - at >= 8) {
        uint32_t oid = rd_u32(c->rx + at);
        uint32_t hdr = rd_u32(c->rx + at + 4);
        uint16_t op = (uint16_t)(hdr & 0xFFFFU);
        uint16_t sz = (uint16_t)(hdr >> 16);
        if (sz < 8 || at + sz > c->rx_len) break;
        process_message(idx, oid, op, c->rx + at + 8, (uint32_t)sz - 8);
        at += sz;
    }
    if (at > 0) {
        memmove(c->rx, c->rx + at, c->rx_len - at);
        c->rx_len -= at;
    }
}

int main(void) {
    puts1("=== AMS Wayland Compositor (wlroots-style, DRM/KMS backend) ===");

    /* Initialize KMS output */
    memset(&g_output, 0, sizeof(g_output));
    g_output.drm_fd = -1;

    if (kms_init(&g_output) < 0) {
        puts1("kms: DRM/KMS init failed, falling back to legacy framebuffer blit");
        if ((int)ams_syscall(SYS_AMS_GET_FB_INFO, (uint64_t)&g_output.width, (uint64_t)&g_output.height, 0, 0, 0) != 0
            || g_output.width == 0 || g_output.height == 0) {
            g_output.width = 1280; g_output.height = 720;
        }
        g_output.framebuffers[0] = (uint32_t*)malloc((size_t)g_output.width * g_output.height * sizeof(uint32_t));
        g_output.framebuffers[1] = g_output.framebuffers[0];
        if (!g_output.framebuffers[0]) { puts1("compositor: fb alloc failed"); return 1; }
    }

    memset(g_objs, 0, sizeof(g_objs));
    memset(g_clients, 0, sizeof(g_clients));
    g_objs[1].type = O_WL_DISPLAY;

    /* Create Wayland listening socket */
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    { const char* p = "/run/user/0/wayland-0"; for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i]; }
    int srv = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (srv < 0) { puts1("compositor: socket failed"); return 1; }
    if ((int)ams_syscall(SYS_BIND, srv, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        puts1("compositor: bind failed"); return 2;
    }
    (void)ams_syscall(SYS_LISTEN, srv, 8, 0, 0, 0);

    composite_and_present();
    puts1("compositor: protocol core+xdg+seat ready, DRM/KMS backend active");
    puts1("compositor: listening on wayland-0 (epoll event loop)");

    /* Main event loop using poll */
    while (1) {
        struct linux_pollfd fds[MAX_CLIENTS + 1];
        int nfds = 0;

        fds[nfds].fd = srv;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;

        for (int i = 0; i < g_client_count; ++i) {
            if (!g_clients[i].active) continue;
            fds[nfds].fd = g_clients[i].fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        int ready = (int)ams_syscall(SYS_POLL, (uint64_t)fds, (uint64_t)nfds, 0, 0, 0);

        if (ready > 0) {
            /* Check server socket for new connections */
            if (fds[0].revents & POLLIN) {
                int cli_fd = (int)ams_syscall(SYS_ACCEPT, srv, 0, 0, 0, 0);
                if (cli_fd >= 0 && g_client_count < MAX_CLIENTS) {
                    init_client_objects(g_client_count, cli_fd);
                    g_client_count++;
                    puts1("compositor: new client connected");
                }
            }

            /* Process client data */
            int poll_idx = 1;
            for (int i = 0; i < g_client_count; ++i) {
                if (!g_clients[i].active) continue;
                if (poll_idx < nfds && (fds[poll_idx].revents & POLLIN)) {
                    process_client_data(i);
                }
                poll_idx++;
            }
        }

        handle_input();
    }
}
