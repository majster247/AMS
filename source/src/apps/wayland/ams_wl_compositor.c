#include "ams_syscall.h"
int main(void){ return 0; }
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32

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
typedef struct wl_obj_state { uint32_t type, pool_id, offset, format, attached_buffer_id, role_id, frame_callback_id, client_fd; int fd; uint8_t* map; uint32_t size; int32_t width, height, stride; } wl_obj_state;
typedef struct wl_fd_queue { int data[WL_FDQ_CAP]; uint32_t head, tail; } wl_fd_queue;
typedef struct wl_client_state { int fd; uint32_t pointer_id, keyboard_id, focused_surface, serial; } wl_client_state;

static wl_obj_state g_objs[WL_OBJECT_MAX];
static wl_client_state g_client = {0};
static uint32_t* wl_fb = 0;
static uint32_t wl_fb_w = 1280, wl_fb_h = 720;
static uint32_t g_pointer_x = 80, g_pointer_y = 80;
static uint8_t g_pointer_buttons = 0;

static void puts1(const char* s) { int n = 0; while (s[n]) ++n; ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0); ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0); }
static uint32_t now_ms(void) { struct linux_timespec_local ts; if ((long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0) != 0) return 0; return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
static uint32_t rd_u32(const uint8_t* p) { return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr_u32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)(v&0xFF); p[1]=(uint8_t)((v>>8)&0xFF); p[2]=(uint8_t)((v>>16)&0xFF); p[3]=(uint8_t)((v>>24)&0xFF); }
static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) { wr_u32(out + at, v); return at + 4; }
static uint32_t append_i32(uint8_t* out, uint32_t at, int32_t v) { wr_u32(out + at, (uint32_t)v); return at + 4; }
static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) { uint32_t len=0; while (s[len]) ++len; at=append_u32(out,at,len+1); memcpy(out+at,s,len); out[at+len]=0; at+=len+1; while(at&3U) out[at++]=0; return at; }

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) { struct linux_iovec iov = {(void*)data, len}; uint8_t control[32] = {0}; struct linux_msghdr msg = {0}; msg.msg_iov = &iov; msg.msg_iovlen = 1; if (send_fd >= 0) { struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control; ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int); ch->cmsg_level = SOL_SOCKET; ch->cmsg_type = SCM_RIGHTS; *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd; msg.msg_control = control; msg.msg_controllen = ch->cmsg_len; } return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0); }
static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) { struct linux_iovec iov = {data, cap}; uint8_t control[64] = {0}; struct linux_msghdr msg = {0}; msg.msg_iov = &iov; msg.msg_iovlen = 1; msg.msg_control = control; msg.msg_controllen = sizeof(control); *recv_fd = -1; int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0); if (rc <= 0) return rc; if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) { struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control; if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS) *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr)); } return rc; }
static void fdq_init(wl_fd_queue* q){ q->head=0; q->tail=0; for(uint32_t i=0;i<WL_FDQ_CAP;++i) q->data[i]=-1; }
static int fdq_push(wl_fd_queue* q,int fd){ uint32_t n=(q->tail+1U)%WL_FDQ_CAP; if(n==q->head) return -1; q->data[q->tail]=fd; q->tail=n; return 0; }
static int fdq_pop(wl_fd_queue* q){ if(q->head==q->tail) return -1; int fd=q->data[q->head]; q->head=(q->head+1U)%WL_FDQ_CAP; return fd; }

static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size){ wr_u32(pkt, obj_id); wr_u32(pkt+4, ((uint32_t)size<<16)|opcode); }
static void send_callback_done(int fd, uint32_t cb_id){ uint8_t pkt[12]={0}; send_event_header(pkt, cb_id, 0, 12); wr_u32(pkt+8, now_ms()); (void)send_packet(fd,pkt,12,-1); }
static void send_buffer_release(int fd, uint32_t id){ uint8_t pkt[8]={0}; send_event_header(pkt,id,0,8); (void)send_packet(fd,pkt,8,-1); }
static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) { uint8_t pkt[256]={0}; uint32_t at=0; at=append_u32(pkt,at,reg_id); uint32_t hdr=at; at=append_u32(pkt,at,0); at=append_u32(pkt,at,name); at=append_string(pkt,at,iface); at=append_u32(pkt,at,version); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1); }

static void draw_shell_background(void){ if(!wl_fb) return; for(uint32_t y=0;y<wl_fb_h;++y){ uint32_t c=(y<36)?0x1E2733:0x151C26; for(uint32_t x=0;x<wl_fb_w;++x) wl_fb[y*wl_fb_w+x]=c; } }
static void draw_pointer(void){ for(uint32_t y=0;y<12;++y) for(uint32_t x=0;x<10;++x){ uint32_t px=g_pointer_x+x, py=g_pointer_y+y; if(px>=wl_fb_w||py>=wl_fb_h) continue; if(x<=y) wl_fb[py*wl_fb_w+px]=0xFFFFFF; } }
static void redraw(void){ draw_shell_background(); draw_pointer(); (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0); }

static void present_surface(uint32_t sid){
    if(sid>=WL_OBJECT_MAX) return;
    wl_obj_state* s=&g_objs[sid]; if(s->type!=O_WL_SURFACE||!s->attached_buffer_id) return;
    wl_obj_state* b=&g_objs[s->attached_buffer_id]; if(b->type!=O_WL_BUFFER||!b->pool_id||b->pool_id>=WL_OBJECT_MAX) return;
    wl_obj_state* p=&g_objs[b->pool_id]; if(p->type!=O_WL_SHM_POOL||!p->map) return;
    draw_shell_background();
    uint32_t cw=(b->width>0&&(uint32_t)b->width<wl_fb_w)?(uint32_t)b->width:wl_fb_w;
    uint32_t ch=(b->height>0&&(uint32_t)b->height<wl_fb_h)?(uint32_t)b->height:wl_fb_h;
    for(uint32_t y=0;y<ch;++y){ uint32_t off=b->offset+y*(uint32_t)b->stride; if(off+cw*4U>p->size) break; memcpy(&wl_fb[y*wl_fb_w], p->map+off, cw*4U); }
    draw_pointer();
    (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0);
    if(s->frame_callback_id && s->frame_callback_id<WL_OBJECT_MAX){ send_callback_done((int)s->client_fd,s->frame_callback_id); g_objs[s->frame_callback_id].type=0; s->frame_callback_id=0; }
    send_buffer_release((int)s->client_fd, s->attached_buffer_id);
}

static void clear_objects(int fd){ memset(g_objs,0,sizeof(g_objs)); memset(&g_client,0,sizeof(g_client)); g_client.fd=fd; g_objs[1].type=O_WL_DISPLAY; g_objs[2].type=O_WL_COMPOSITOR; g_objs[3].type=O_WL_SHM; }
static void send_output_info(int fd, uint32_t oid){ uint8_t pkt[128]={0}; uint32_t at=0; at=append_u32(pkt,at,oid); uint32_t hdr=at; at=append_u32(pkt,at,0); at=append_i32(pkt,at,0); at=append_i32(pkt,at,0); at=append_i32(pkt,at,300); at=append_i32(pkt,at,170); at=append_u32(pkt,at,1); at=append_string(pkt,at,"AMS"); at=append_string(pkt,at,"Virtual-0"); at=append_i32(pkt,at,0); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1); uint8_t mode[24]={0}; send_event_header(mode,oid,1,20); wr_u32(mode+8,3); wr_u32(mode+12,wl_fb_w); wr_u32(mode+16,wl_fb_h); wr_u32(mode+20,60000); (void)send_packet(fd,mode,24,-1); uint8_t scale[12]={0}; send_event_header(scale,oid,3,12); wr_u32(scale+8,1); (void)send_packet(fd,scale,12,-1); uint8_t done[8]={0}; send_event_header(done,oid,2,8); (void)send_packet(fd,done,8,-1); }
static void send_seat_info(int fd, uint32_t sid){ uint8_t caps[12]={0}; send_event_header(caps,sid,0,12); wr_u32(caps+8,3); (void)send_packet(fd,caps,12,-1); uint8_t name[64]={0}; uint32_t at=0; at=append_u32(name,at,sid); uint32_t hdr=at; at=append_u32(name,at,0); at=append_string(name,at,"seat0"); wr_u32(name+hdr,(at<<16)|1); (void)send_packet(fd,name,at,-1); }
static void send_pointer_enter(int fd, uint32_t pid, uint32_t sid){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,0,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,g_pointer_x<<8); wr_u32(pkt+20,g_pointer_y<<8); (void)send_packet(fd,pkt,24,-1); }
static void send_pointer_motion(int fd, uint32_t pid){ uint8_t pkt[20]={0}; send_event_header(pkt,pid,2,20); wr_u32(pkt+8,now_ms()); wr_u32(pkt+12,g_pointer_x<<8); wr_u32(pkt+16,g_pointer_y<<8); (void)send_packet(fd,pkt,20,-1); }
static void send_pointer_button(int fd,uint32_t pid,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,0x110); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }
static void send_keyboard_enter(int fd,uint32_t kid,uint32_t sid){ uint8_t pkt[20]={0}; send_event_header(pkt,kid,1,20); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,0); (void)send_packet(fd,pkt,20,-1); }
static void send_keyboard_key(int fd,uint32_t kid,uint32_t key,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,kid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,key); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }

static void handle_input(void){
    int should_redraw = 0;
    uint64_t mev=ams_syscall(SYS_AMS_GET_MOUSE_EVENT,0,0,0,0,0);
    if(mev && g_client.pointer_id && g_client.focused_surface){
        uint32_t old_x = g_pointer_x;
        uint32_t old_y = g_pointer_y;
        g_pointer_x=(uint32_t)(mev&0xFFFFU); g_pointer_y=(uint32_t)((mev>>16)&0xFFFFU);
        uint8_t buttons=(uint8_t)((mev>>32)&0xFFU); uint8_t old=g_pointer_buttons; g_pointer_buttons=buttons;
        send_pointer_motion(g_client.fd,g_client.pointer_id);
        if(old!=g_pointer_buttons) send_pointer_button(g_client.fd,g_client.pointer_id,(g_pointer_buttons&1U)?1U:0U);
        if (old_x != g_pointer_x || old_y != g_pointer_y || old != g_pointer_buttons) should_redraw = 1;
    }
    uint64_t kev=ams_syscall(SYS_AMS_GET_KEY,0,0,0,0,0);
    if(kev && g_client.keyboard_id && g_client.focused_surface){ int32_t k=(int32_t)kev; uint32_t st=1; if(k<0){st=0;k=-k;} send_keyboard_key(g_client.fd,g_client.keyboard_id,(uint32_t)k,st); }
    if (should_redraw) redraw();
}

static void process_message(int fd, wl_fd_queue* fdq, uint32_t oid, uint16_t op, const uint8_t* p, uint32_t n){
    if(oid>=WL_OBJECT_MAX) return;
    wl_obj_state* o=&g_objs[oid];
    if(oid==1 && o->type==O_WL_DISPLAY){ if(op==0 && n>=4){ uint32_t cb=rd_u32(p); if(cb&&cb<WL_OBJECT_MAX){ g_objs[cb].type=O_WL_CALLBACK; send_callback_done(fd,cb);} } if(op==1 && n>=4){ uint32_t rid=rd_u32(p); if(rid&&rid<WL_OBJECT_MAX){ g_objs[rid].type=O_WL_REGISTRY; send_registry_global(fd,rid,1,"wl_compositor",4); send_registry_global(fd,rid,2,"wl_shm",1); send_registry_global(fd,rid,3,"wl_output",2); send_registry_global(fd,rid,4,"wl_seat",5); send_registry_global(fd,rid,5,"xdg_wm_base",1);} } return; }
    if(o->type==O_WL_REGISTRY){ if(op==0 && n>=16){ uint32_t name=rd_u32(p); uint32_t sl=rd_u32(p+4); uint32_t sp=(sl+3U)&~3U; if(n<4+4+sp+8) return; uint32_t nid=rd_u32(p+12+sp); if(!nid||nid>=WL_OBJECT_MAX) return; if(name==1) g_objs[nid].type=O_WL_COMPOSITOR; else if(name==2){ g_objs[nid].type=O_WL_SHM; uint8_t f[12]={0}; send_event_header(f,nid,0,12); wr_u32(f+8,0); (void)send_packet(fd,f,12,-1); } else if(name==3){ g_objs[nid].type=O_WL_OUTPUT; send_output_info(fd,nid); } else if(name==4){ g_objs[nid].type=O_WL_SEAT; send_seat_info(fd,nid); } else if(name==5){ g_objs[nid].type=O_XDG_WM_BASE; uint8_t ping[12]={0}; send_event_header(ping,nid,0,12); wr_u32(ping+8,++g_client.serial); (void)send_packet(fd,ping,12,-1); } } return; }
    if(o->type==O_WL_COMPOSITOR){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_SURFACE; g_objs[nid].client_fd=(uint32_t)fd; } } return; }
    if(o->type==O_WL_SHM){ if(op==0&&n>=8){ uint32_t nid=rd_u32(p), sz=rd_u32(p+4); int passed=fdq_pop(fdq); if(!nid||nid>=WL_OBJECT_MAX||passed<0||sz==0) return; g_objs[nid].type=O_WL_SHM_POOL; g_objs[nid].fd=passed; g_objs[nid].size=sz; g_objs[nid].map=(uint8_t*)mmap(0,sz,PROT_READ,MAP_SHARED,passed,0); if((uint64_t)g_objs[nid].map>(uint64_t)-4096LL) g_objs[nid].map=0;} return; }
    if(o->type==O_WL_SHM_POOL){ if(op==0&&n>=24){ uint32_t nid=rd_u32(p); if(!nid||nid>=WL_OBJECT_MAX) return; g_objs[nid].type=O_WL_BUFFER; g_objs[nid].pool_id=oid; g_objs[nid].offset=rd_u32(p+4); g_objs[nid].width=(int32_t)rd_u32(p+8); g_objs[nid].height=(int32_t)rd_u32(p+12); g_objs[nid].stride=(int32_t)rd_u32(p+16); g_objs[nid].format=rd_u32(p+20); g_objs[nid].client_fd=(uint32_t)fd; } return; }
    if(o->type==O_WL_SURFACE){ if(op==1&&n>=12) o->attached_buffer_id=rd_u32(p); else if(op==3&&n>=4){ uint32_t cb=rd_u32(p); o->frame_callback_id=cb; if(cb&&cb<WL_OBJECT_MAX) g_objs[cb].type=O_WL_CALLBACK; } else if(op==6){ g_client.focused_surface=oid; present_surface(oid); if(g_client.pointer_id) send_pointer_enter(fd,g_client.pointer_id,oid); if(g_client.keyboard_id) send_keyboard_enter(fd,g_client.keyboard_id,oid); } return; }
    if(o->type==O_WL_SEAT){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_POINTER; g_client.pointer_id=nid; }} else if(op==1&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_KEYBOARD; g_client.keyboard_id=nid; }} return; }
    if(o->type==O_XDG_WM_BASE){ if(op==1&&n>=8){ uint32_t xs=rd_u32(p), sid=rd_u32(p+4); if(xs&&xs<WL_OBJECT_MAX&&sid&&sid<WL_OBJECT_MAX){ g_objs[xs].type=O_XDG_SURFACE; g_objs[xs].role_id=sid; g_objs[sid].role_id=xs; uint8_t cfg[12]={0}; send_event_header(cfg,xs,0,12); wr_u32(cfg+8,++g_client.serial); (void)send_packet(fd,cfg,12,-1); }} return; }
    if(o->type==O_XDG_SURFACE){ if(op==1&&n>=4){ uint32_t tl=rd_u32(p); if(tl&&tl<WL_OBJECT_MAX){ g_objs[tl].type=O_XDG_TOPLEVEL; uint8_t tcfg[20]={0}; send_event_header(tcfg,tl,0,20); wr_u32(tcfg+8,wl_fb_w); wr_u32(tcfg+12,wl_fb_h); wr_u32(tcfg+16,0); (void)send_packet(fd,tcfg,20,-1); uint8_t scfg[12]={0}; send_event_header(scfg,oid,0,12); wr_u32(scfg+8,++g_client.serial); (void)send_packet(fd,scfg,12,-1); }} return; }
    if(o->type==O_WL_BUFFER && op==0) o->type=0;
}

static int bootstrap_local_shell(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int fd=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(fd<0) return -1;
    if((int)ams_syscall(SYS_CONNECT,(uint64_t)fd,(uint64_t)&addr,sizeof(addr),0,0)<0) return -2;
    const uint32_t reg=40, comp=41, shm=42, surf=43, pool=44, buf=45, xwm=46, xsurf=47, xtop=48;
    uint8_t m[512]={0}; uint32_t at=0, h=0;
    at=append_u32(m,at,1); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,reg); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,1); at=append_string(m,at,"wl_compositor"); at=append_u32(m,at,4); at=append_u32(m,at,comp); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,2); at=append_string(m,at,"wl_shm"); at=append_u32(m,at,1); at=append_u32(m,at,shm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,5); at=append_string(m,at,"xdg_wm_base"); at=append_u32(m,at,1); at=append_u32(m,at,xwm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,comp); at=append_u32(m,at,(12U<<16)|0U); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xwm); at=append_u32(m,at,(16U<<16)|1U); at=append_u32(m,at,xsurf); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xsurf); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,xtop); (void)send_packet(fd,m,at,-1);
    int w=960,hh=540,stride=w*4,size=stride*hh; int shmfd=(int)ams_syscall(SYS_MEMFD_CREATE,(uint64_t)"wl-shell",0,0,0,0); if(shmfd<0) return -3;
    if((int)ams_syscall(SYS_FTRUNCATE,(uint64_t)shmfd,(uint64_t)size,0,0,0)<0) return -4;
    uint32_t* pix=(uint32_t*)mmap(0,(size_t)size,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0); if((uint64_t)pix>(uint64_t)-4096LL) return -5;
    for(int y=0;y<hh;++y) for(int x=0;x<w;++x) pix[y*w+x]=(y<40)?0x2B394C:0x1A2230;
    at=0; at=append_u32(m,at,shm); at=append_u32(m,at,(16U<<16)|0U); at=append_u32(m,at,pool); at=append_u32(m,at,(uint32_t)size); (void)send_packet(fd,m,at,shmfd);
    at=0; at=append_u32(m,at,pool); at=append_u32(m,at,(32U<<16)|0U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,(uint32_t)w); at=append_u32(m,at,(uint32_t)hh); at=append_u32(m,at,(uint32_t)stride); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(20U<<16)|1U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(8U<<16)|6U); (void)send_packet(fd,m,at,-1);
    return fd;
}

static void handle_client(int cli){
    uint8_t rx[WL_RX_CAP]; uint32_t rx_len=0; wl_fd_queue fdq; fdq_init(&fdq); clear_objects(cli);
    puts1("wl-compositor: client connected");
    while(1){ int pass=-1; int n=recv_packet(cli,rx+rx_len,WL_RX_CAP-rx_len,&pass); if(n==0) break; if(n<0){ handle_input(); continue; } if(pass>=0) (void)fdq_push(&fdq,pass); rx_len+=(uint32_t)n; uint32_t at=0; while(rx_len-at>=8){ uint32_t oid=rd_u32(rx+at), hdr=rd_u32(rx+at+4); uint16_t op=(uint16_t)(hdr&0xFFFFU), sz=(uint16_t)(hdr>>16); if(sz<8||at+sz>rx_len) break; process_message(cli,&fdq,oid,op,rx+at+8,(uint32_t)sz-8); at+=sz; } if(at>0){ memmove(rx,rx+at,rx_len-at); rx_len-=at; } handle_input(); }
    puts1("wl-compositor: client disconnected");
}

int main(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int srv=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(srv<0){ puts1("wl-compositor: socket failed"); return 1; }
    if((int)ams_syscall(SYS_BIND,srv,(uint64_t)&addr,sizeof(addr),0,0)<0){ puts1("wl-compositor: bind failed"); return 2; }
    (void)ams_syscall(SYS_LISTEN,srv,8,0,0,0); puts1("wl-compositor: listening on wayland-0");
    if((int)ams_syscall(SYS_AMS_GET_FB_INFO,(uint64_t)&wl_fb_w,(uint64_t)&wl_fb_h,0,0,0)!=0||wl_fb_w==0||wl_fb_h==0){ wl_fb_w=1280; wl_fb_h=720; }
    wl_fb=(uint32_t*)malloc((size_t)wl_fb_w*(size_t)wl_fb_h*sizeof(uint32_t)); if(!wl_fb){ puts1("wl-compositor: fb buffer alloc failed"); return 3; }
    redraw();
    puts1("wl-compositor: protocol core+xdg+seat ready");
    if(bootstrap_local_shell()>=0) puts1("wl-compositor: local shell bootstrap queued"); else puts1("wl-compositor: local shell bootstrap failed");
    while(1){ int cli=(int)ams_syscall(SYS_ACCEPT,srv,0,0,0,0); if(cli<0){ handle_input(); continue; } handle_client(cli); }
}
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32

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

typedef struct wl_obj_state { uint32_t type, pool_id, offset, format, attached_buffer_id, role_id, frame_callback_id, client_fd; int fd; uint8_t* map; uint32_t size; int32_t width, height, stride; } wl_obj_state;
typedef struct wl_fd_queue { int data[WL_FDQ_CAP]; uint32_t head, tail; } wl_fd_queue;
typedef struct wl_client_state { int fd; uint32_t pointer_id, keyboard_id, focused_surface, serial; } wl_client_state;

static wl_obj_state g_objs[WL_OBJECT_MAX];
static wl_client_state g_client = {0};
static uint32_t* wl_fb = 0;
static uint32_t wl_fb_w = 1280, wl_fb_h = 720;
static uint32_t g_pointer_x = 80, g_pointer_y = 80;
static uint8_t g_pointer_buttons = 0;

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
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET;
        ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control;
        msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) {
    struct linux_iovec iov = {data, cap};
    uint8_t control[64] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    *recv_fd = -1;
    int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
    if (rc <= 0) return rc;
    if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS) {
            *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr));
        }
    }
    return rc;
}

static void fdq_init(wl_fd_queue* q){ q->head=0; q->tail=0; for(uint32_t i=0;i<WL_FDQ_CAP;++i) q->data[i]=-1; }
static int fdq_push(wl_fd_queue* q,int fd){ uint32_t n=(q->tail+1U)%WL_FDQ_CAP; if(n==q->head) return -1; q->data[q->tail]=fd; q->tail=n; return 0; }
static int fdq_pop(wl_fd_queue* q){ if(q->head==q->tail) return -1; int fd=q->data[q->head]; q->head=(q->head+1U)%WL_FDQ_CAP; return fd; }

static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size){ wr_u32(pkt, obj_id); wr_u32(pkt+4, ((uint32_t)size<<16)|opcode); }
static void send_callback_done(int fd, uint32_t cb_id){ uint8_t pkt[12]={0}; send_event_header(pkt, cb_id, 0, 12); wr_u32(pkt+8, now_ms()); (void)send_packet(fd,pkt,12,-1); }
static void send_buffer_release(int fd, uint32_t id){ uint8_t pkt[8]={0}; send_event_header(pkt,id,0,8); (void)send_packet(fd,pkt,8,-1); }

static void draw_shell_background(void){ if(!wl_fb) return; for(uint32_t y=0;y<wl_fb_h;++y){ uint32_t c=(y<36)?0x1E2733:0x151C26; for(uint32_t x=0;x<wl_fb_w;++x) wl_fb[y*wl_fb_w+x]=c; } }
static void draw_pointer(void){ for(uint32_t y=0;y<12;++y) for(uint32_t x=0;x<10;++x){ uint32_t px=g_pointer_x+x, py=g_pointer_y+y; if(px>=wl_fb_w||py>=wl_fb_h) continue; if(x<=y) wl_fb[py*wl_fb_w+px]=0xFFFFFF; } }
static void redraw(void){ draw_shell_background(); draw_pointer(); (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0); }

static void present_surface(uint32_t sid){
    if(sid>=WL_OBJECT_MAX) return;
    wl_obj_state* s=&g_objs[sid]; if(s->type!=O_WL_SURFACE||!s->attached_buffer_id) return;
    wl_obj_state* b=&g_objs[s->attached_buffer_id]; if(b->type!=O_WL_BUFFER||!b->pool_id||b->pool_id>=WL_OBJECT_MAX) return;
    wl_obj_state* p=&g_objs[b->pool_id]; if(p->type!=O_WL_SHM_POOL||!p->map) return;
    draw_shell_background();
    uint32_t cw=(b->width>0&&(uint32_t)b->width<wl_fb_w)?(uint32_t)b->width:wl_fb_w;
    uint32_t ch=(b->height>0&&(uint32_t)b->height<wl_fb_h)?(uint32_t)b->height:wl_fb_h;
    for(uint32_t y=0;y<ch;++y){ uint32_t off=b->offset+y*(uint32_t)b->stride; if(off+cw*4U>p->size) break; memcpy(&wl_fb[y*wl_fb_w], p->map+off, cw*4U); }
    draw_pointer();
    (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0);
    if(s->frame_callback_id && s->frame_callback_id<WL_OBJECT_MAX){ send_callback_done((int)s->client_fd,s->frame_callback_id); g_objs[s->frame_callback_id].type=0; s->frame_callback_id=0; }
    send_buffer_release((int)s->client_fd, s->attached_buffer_id);
}

static void clear_objects(int fd){ memset(g_objs,0,sizeof(g_objs)); memset(&g_client,0,sizeof(g_client)); g_client.fd=fd; g_objs[1].type=O_WL_DISPLAY; g_objs[2].type=O_WL_COMPOSITOR; g_objs[3].type=O_WL_SHM; }

static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) {
    uint8_t pkt[256]={0}; uint32_t at=0; at=append_u32(pkt,at,reg_id); uint32_t hdr=at; at=append_u32(pkt,at,0); at=append_u32(pkt,at,name); at=append_string(pkt,at,iface); at=append_u32(pkt,at,version); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1);
}

static void send_output_info(int fd, uint32_t oid){
    uint8_t pkt[128]={0}; uint32_t at=0; at=append_u32(pkt,at,oid); uint32_t hdr=at; at=append_u32(pkt,at,0);
    at=append_i32(pkt,at,0); at=append_i32(pkt,at,0); at=append_i32(pkt,at,300); at=append_i32(pkt,at,170); at=append_u32(pkt,at,1); at=append_string(pkt,at,"AMS"); at=append_string(pkt,at,"Virtual-0"); at=append_i32(pkt,at,0); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1);
    uint8_t mode[24]={0}; send_event_header(mode,oid,1,20); wr_u32(mode+8,3); wr_u32(mode+12,wl_fb_w); wr_u32(mode+16,wl_fb_h); wr_u32(mode+20,60000); (void)send_packet(fd,mode,24,-1);
    uint8_t scale[12]={0}; send_event_header(scale,oid,3,12); wr_u32(scale+8,1); (void)send_packet(fd,scale,12,-1);
    uint8_t done[8]={0}; send_event_header(done,oid,2,8); (void)send_packet(fd,done,8,-1);
}

static void send_seat_info(int fd, uint32_t sid){
    uint8_t caps[12]={0}; send_event_header(caps,sid,0,12); wr_u32(caps+8,3); (void)send_packet(fd,caps,12,-1);
    uint8_t name[64]={0}; uint32_t at=0; at=append_u32(name,at,sid); uint32_t hdr=at; at=append_u32(name,at,0); at=append_string(name,at,"seat0"); wr_u32(name+hdr,(at<<16)|1); (void)send_packet(fd,name,at,-1);
}

static void send_pointer_enter(int fd, uint32_t pid, uint32_t sid){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,0,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,g_pointer_x<<8); wr_u32(pkt+20,g_pointer_y<<8); (void)send_packet(fd,pkt,24,-1); }
static void send_pointer_motion(int fd, uint32_t pid){ uint8_t pkt[20]={0}; send_event_header(pkt,pid,2,20); wr_u32(pkt+8,now_ms()); wr_u32(pkt+12,g_pointer_x<<8); wr_u32(pkt+16,g_pointer_y<<8); (void)send_packet(fd,pkt,20,-1); }
static void send_pointer_button(int fd,uint32_t pid,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,0x110); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }
static void send_keyboard_enter(int fd,uint32_t kid,uint32_t sid){ uint8_t pkt[20]={0}; send_event_header(pkt,kid,1,20); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,0); (void)send_packet(fd,pkt,20,-1); }
static void send_keyboard_key(int fd,uint32_t kid,uint32_t key,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,kid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,key); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }

static void handle_input(void){
    int should_redraw = 0;
    uint64_t mev=ams_syscall(SYS_AMS_GET_MOUSE_EVENT,0,0,0,0,0);
    if(mev && g_client.pointer_id && g_client.focused_surface){
        uint32_t old_x = g_pointer_x;
        uint32_t old_y = g_pointer_y;
        g_pointer_x=(uint32_t)(mev&0xFFFFU); g_pointer_y=(uint32_t)((mev>>16)&0xFFFFU);
        uint8_t buttons=(uint8_t)((mev>>32)&0xFFU); uint8_t old=g_pointer_buttons; g_pointer_buttons=buttons;
        send_pointer_motion(g_client.fd,g_client.pointer_id);
        if(old!=g_pointer_buttons) send_pointer_button(g_client.fd,g_client.pointer_id,(g_pointer_buttons&1U)?1U:0U);
        if (old_x != g_pointer_x || old_y != g_pointer_y || old != g_pointer_buttons) should_redraw = 1;
    }
    uint64_t kev=ams_syscall(SYS_AMS_GET_KEY,0,0,0,0,0);
    if(kev && g_client.keyboard_id && g_client.focused_surface){ int32_t k=(int32_t)kev; uint32_t st=1; if(k<0){st=0;k=-k;} send_keyboard_key(g_client.fd,g_client.keyboard_id,(uint32_t)k,st); }
    if (should_redraw) redraw();
}

static void process_message(int fd, wl_fd_queue* fdq, uint32_t oid, uint16_t op, const uint8_t* p, uint32_t n){
    if(oid>=WL_OBJECT_MAX) return;
    wl_obj_state* o=&g_objs[oid];
    if(oid==1 && o->type==O_WL_DISPLAY){ if(op==0 && n>=4){ uint32_t cb=rd_u32(p); if(cb&&cb<WL_OBJECT_MAX){ g_objs[cb].type=O_WL_CALLBACK; send_callback_done(fd,cb);} } if(op==1 && n>=4){ uint32_t rid=rd_u32(p); if(rid&&rid<WL_OBJECT_MAX){ g_objs[rid].type=O_WL_REGISTRY; send_registry_global(fd,rid,1,"wl_compositor",4); send_registry_global(fd,rid,2,"wl_shm",1); send_registry_global(fd,rid,3,"wl_output",2); send_registry_global(fd,rid,4,"wl_seat",5); send_registry_global(fd,rid,5,"xdg_wm_base",1);} } return; }
    if(o->type==O_WL_REGISTRY){ if(op==0 && n>=16){ uint32_t name=rd_u32(p); uint32_t sl=rd_u32(p+4); uint32_t sp=(sl+3U)&~3U; if(n<4+4+sp+8) return; uint32_t nid=rd_u32(p+12+sp); if(!nid||nid>=WL_OBJECT_MAX) return; if(name==1) g_objs[nid].type=O_WL_COMPOSITOR; else if(name==2){ g_objs[nid].type=O_WL_SHM; uint8_t f[12]={0}; send_event_header(f,nid,0,12); wr_u32(f+8,0); (void)send_packet(fd,f,12,-1); } else if(name==3){ g_objs[nid].type=O_WL_OUTPUT; send_output_info(fd,nid); } else if(name==4){ g_objs[nid].type=O_WL_SEAT; send_seat_info(fd,nid); } else if(name==5){ g_objs[nid].type=O_XDG_WM_BASE; uint8_t ping[12]={0}; send_event_header(ping,nid,0,12); wr_u32(ping+8,++g_client.serial); (void)send_packet(fd,ping,12,-1); } } return; }
    if(o->type==O_WL_COMPOSITOR){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_SURFACE; g_objs[nid].client_fd=(uint32_t)fd; } } return; }
    if(o->type==O_WL_SHM){ if(op==0&&n>=8){ uint32_t nid=rd_u32(p), sz=rd_u32(p+4); int passed=fdq_pop(fdq); if(!nid||nid>=WL_OBJECT_MAX||passed<0||sz==0) return; g_objs[nid].type=O_WL_SHM_POOL; g_objs[nid].fd=passed; g_objs[nid].size=sz; g_objs[nid].map=(uint8_t*)mmap(0,sz,PROT_READ,MAP_SHARED,passed,0); if((uint64_t)g_objs[nid].map>(uint64_t)-4096LL) g_objs[nid].map=0;} return; }
    if(o->type==O_WL_SHM_POOL){ if(op==0&&n>=24){ uint32_t nid=rd_u32(p); if(!nid||nid>=WL_OBJECT_MAX) return; g_objs[nid].type=O_WL_BUFFER; g_objs[nid].pool_id=oid; g_objs[nid].offset=rd_u32(p+4); g_objs[nid].width=(int32_t)rd_u32(p+8); g_objs[nid].height=(int32_t)rd_u32(p+12); g_objs[nid].stride=(int32_t)rd_u32(p+16); g_objs[nid].format=rd_u32(p+20); g_objs[nid].client_fd=(uint32_t)fd; } return; }
    if(o->type==O_WL_SURFACE){ if(op==1&&n>=12) o->attached_buffer_id=rd_u32(p); else if(op==3&&n>=4){ uint32_t cb=rd_u32(p); o->frame_callback_id=cb; if(cb&&cb<WL_OBJECT_MAX) g_objs[cb].type=O_WL_CALLBACK; } else if(op==6){ g_client.focused_surface=oid; present_surface(oid); if(g_client.pointer_id) send_pointer_enter(fd,g_client.pointer_id,oid); if(g_client.keyboard_id) send_keyboard_enter(fd,g_client.keyboard_id,oid); } return; }
    if(o->type==O_WL_SEAT){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_POINTER; g_client.pointer_id=nid; }} else if(op==1&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_KEYBOARD; g_client.keyboard_id=nid; }} return; }
    if(o->type==O_XDG_WM_BASE){ if(op==1&&n>=8){ uint32_t xs=rd_u32(p), sid=rd_u32(p+4); if(xs&&xs<WL_OBJECT_MAX&&sid&&sid<WL_OBJECT_MAX){ g_objs[xs].type=O_XDG_SURFACE; g_objs[xs].role_id=sid; g_objs[sid].role_id=xs; uint8_t cfg[12]={0}; send_event_header(cfg,xs,0,12); wr_u32(cfg+8,++g_client.serial); (void)send_packet(fd,cfg,12,-1); }} return; }
    if(o->type==O_XDG_SURFACE){ if(op==1&&n>=4){ uint32_t tl=rd_u32(p); if(tl&&tl<WL_OBJECT_MAX){ g_objs[tl].type=O_XDG_TOPLEVEL; uint8_t tcfg[20]={0}; send_event_header(tcfg,tl,0,20); wr_u32(tcfg+8,wl_fb_w); wr_u32(tcfg+12,wl_fb_h); wr_u32(tcfg+16,0); (void)send_packet(fd,tcfg,20,-1); uint8_t scfg[12]={0}; send_event_header(scfg,oid,0,12); wr_u32(scfg+8,++g_client.serial); (void)send_packet(fd,scfg,12,-1); }} return; }
    if(o->type==O_WL_BUFFER && op==0) o->type=0;
}

static int bootstrap_local_shell(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int fd=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(fd<0) return -1;
    if((int)ams_syscall(SYS_CONNECT,(uint64_t)fd,(uint64_t)&addr,sizeof(addr),0,0)<0) return -2;
    const uint32_t reg=40, comp=41, shm=42, surf=43, pool=44, buf=45, xwm=46, xsurf=47, xtop=48;
    uint8_t m[512]={0}; uint32_t at=0, h=0;
    at=append_u32(m,at,1); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,reg); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,1); at=append_string(m,at,"wl_compositor"); at=append_u32(m,at,4); at=append_u32(m,at,comp); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,2); at=append_string(m,at,"wl_shm"); at=append_u32(m,at,1); at=append_u32(m,at,shm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,5); at=append_string(m,at,"xdg_wm_base"); at=append_u32(m,at,1); at=append_u32(m,at,xwm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,comp); at=append_u32(m,at,(12U<<16)|0U); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xwm); at=append_u32(m,at,(16U<<16)|1U); at=append_u32(m,at,xsurf); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xsurf); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,xtop); (void)send_packet(fd,m,at,-1);
    int w=960,hh=540,stride=w*4,size=stride*hh; int shmfd=(int)ams_syscall(SYS_MEMFD_CREATE,(uint64_t)"wl-shell",0,0,0,0); if(shmfd<0) return -3;
    if((int)ams_syscall(SYS_FTRUNCATE,(uint64_t)shmfd,(uint64_t)size,0,0,0)<0) return -4;
    uint32_t* pix=(uint32_t*)mmap(0,(size_t)size,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0); if((uint64_t)pix>(uint64_t)-4096LL) return -5;
    for(int y=0;y<hh;++y) for(int x=0;x<w;++x) pix[y*w+x]=(y<40)?0x2B394C:0x1A2230;
    at=0; at=append_u32(m,at,shm); at=append_u32(m,at,(16U<<16)|0U); at=append_u32(m,at,pool); at=append_u32(m,at,(uint32_t)size); (void)send_packet(fd,m,at,shmfd);
    at=0; at=append_u32(m,at,pool); at=append_u32(m,at,(32U<<16)|0U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,(uint32_t)w); at=append_u32(m,at,(uint32_t)hh); at=append_u32(m,at,(uint32_t)stride); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(20U<<16)|1U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(8U<<16)|6U); (void)send_packet(fd,m,at,-1);
    return fd;
}

static void handle_client(int cli){
    uint8_t rx[WL_RX_CAP]; uint32_t rx_len=0; wl_fd_queue fdq; fdq_init(&fdq); clear_objects(cli);
    puts1("wl-compositor: client connected");
    while(1){
        int pass=-1; int n=recv_packet(cli,rx+rx_len,WL_RX_CAP-rx_len,&pass);
        if(n==0) break;
        if(n<0){ handle_input(); continue; }
        if(pass>=0) (void)fdq_push(&fdq,pass);
        rx_len+=(uint32_t)n;
        uint32_t at=0; while(rx_len-at>=8){ uint32_t oid=rd_u32(rx+at), hdr=rd_u32(rx+at+4); uint16_t op=(uint16_t)(hdr&0xFFFFU), sz=(uint16_t)(hdr>>16); if(sz<8||at+sz>rx_len) break; process_message(cli,&fdq,oid,op,rx+at+8,(uint32_t)sz-8); at+=sz; }
        if(at>0){ memmove(rx,rx+at,rx_len-at); rx_len-=at; }
        handle_input();
    }
    puts1("wl-compositor: client disconnected");
}

int main(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int srv=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(srv<0){ puts1("wl-compositor: socket failed"); return 1; }
    if((int)ams_syscall(SYS_BIND,srv,(uint64_t)&addr,sizeof(addr),0,0)<0){ puts1("wl-compositor: bind failed"); return 2; }
    (void)ams_syscall(SYS_LISTEN,srv,8,0,0,0); puts1("wl-compositor: listening on wayland-0");
    if((int)ams_syscall(SYS_AMS_GET_FB_INFO,(uint64_t)&wl_fb_w,(uint64_t)&wl_fb_h,0,0,0)!=0||wl_fb_w==0||wl_fb_h==0){ wl_fb_w=1280; wl_fb_h=720; }
    wl_fb=(uint32_t*)malloc((size_t)wl_fb_w*(size_t)wl_fb_h*sizeof(uint32_t)); if(!wl_fb){ puts1("wl-compositor: fb buffer alloc failed"); return 3; }
    redraw();
    puts1("wl-compositor: protocol core+xdg+seat ready");
    if(bootstrap_local_shell()>=0) puts1("wl-compositor: local shell bootstrap queued"); else puts1("wl-compositor: local shell bootstrap failed");
    while(1){ int cli=(int)ams_syscall(SYS_ACCEPT,srv,0,0,0,0); if(cli<0){ handle_input(); continue; } handle_client(cli); }
}
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32

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
typedef struct wl_obj_state { uint32_t type, pool_id, offset, format, attached_buffer_id, role_id, frame_callback_id, client_fd; int fd; uint8_t* map; uint32_t size; int32_t width, height, stride; } wl_obj_state;
typedef struct wl_fd_queue { int data[WL_FDQ_CAP]; uint32_t head, tail; } wl_fd_queue;
typedef struct wl_client_state { int fd; uint32_t pointer_id, keyboard_id, focused_surface, serial; } wl_client_state;

static wl_obj_state g_objs[WL_OBJECT_MAX];
static wl_client_state g_client = {0};
static uint32_t* wl_fb = 0;
static uint32_t wl_fb_w = 1280, wl_fb_h = 720;
static uint32_t g_pointer_x = 80, g_pointer_y = 80;
static uint8_t g_pointer_buttons = 0;

static void puts1(const char* s) { int n = 0; while (s[n]) ++n; ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0); ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0); }
static uint32_t now_ms(void) { struct linux_timespec_local ts; if ((long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0) != 0) return 0; return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
static uint32_t rd_u32(const uint8_t* p) { return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr_u32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)(v&0xFF); p[1]=(uint8_t)((v>>8)&0xFF); p[2]=(uint8_t)((v>>16)&0xFF); p[3]=(uint8_t)((v>>24)&0xFF); }
static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) { wr_u32(out + at, v); return at + 4; }
static uint32_t append_i32(uint8_t* out, uint32_t at, int32_t v) { wr_u32(out + at, (uint32_t)v); return at + 4; }
static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) { uint32_t len=0; while (s[len]) ++len; at=append_u32(out,at,len+1); memcpy(out+at,s,len); out[at+len]=0; at+=len+1; while(at&3U) out[at++]=0; return at; }

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) { struct linux_iovec iov = {(void*)data, len}; uint8_t control[32] = {0}; struct linux_msghdr msg = {0}; msg.msg_iov = &iov; msg.msg_iovlen = 1; if (send_fd >= 0) { struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control; ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int); ch->cmsg_level = SOL_SOCKET; ch->cmsg_type = SCM_RIGHTS; *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd; msg.msg_control = control; msg.msg_controllen = ch->cmsg_len; } return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0); }
static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) { struct linux_iovec iov = {data, cap}; uint8_t control[64] = {0}; struct linux_msghdr msg = {0}; msg.msg_iov = &iov; msg.msg_iovlen = 1; msg.msg_control = control; msg.msg_controllen = sizeof(control); *recv_fd = -1; int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0); if (rc <= 0) return rc; if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) { struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control; if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS) *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr)); } return rc; }
static void fdq_init(wl_fd_queue* q){ q->head=0; q->tail=0; for(uint32_t i=0;i<WL_FDQ_CAP;++i) q->data[i]=-1; }
static int fdq_push(wl_fd_queue* q,int fd){ uint32_t n=(q->tail+1U)%WL_FDQ_CAP; if(n==q->head) return -1; q->data[q->tail]=fd; q->tail=n; return 0; }
static int fdq_pop(wl_fd_queue* q){ if(q->head==q->tail) return -1; int fd=q->data[q->head]; q->head=(q->head+1U)%WL_FDQ_CAP; return fd; }

static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size){ wr_u32(pkt, obj_id); wr_u32(pkt+4, ((uint32_t)size<<16)|opcode); }
static void send_callback_done(int fd, uint32_t cb_id){ uint8_t pkt[12]={0}; send_event_header(pkt, cb_id, 0, 12); wr_u32(pkt+8, now_ms()); (void)send_packet(fd,pkt,12,-1); }
static void send_buffer_release(int fd, uint32_t id){ uint8_t pkt[8]={0}; send_event_header(pkt,id,0,8); (void)send_packet(fd,pkt,8,-1); }
static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) { uint8_t pkt[256]={0}; uint32_t at=0; at=append_u32(pkt,at,reg_id); uint32_t hdr=at; at=append_u32(pkt,at,0); at=append_u32(pkt,at,name); at=append_string(pkt,at,iface); at=append_u32(pkt,at,version); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1); }

static void draw_shell_background(void){ if(!wl_fb) return; for(uint32_t y=0;y<wl_fb_h;++y){ uint32_t c=(y<36)?0x1E2733:0x151C26; for(uint32_t x=0;x<wl_fb_w;++x) wl_fb[y*wl_fb_w+x]=c; } }
static void draw_pointer(void){ for(uint32_t y=0;y<12;++y) for(uint32_t x=0;x<10;++x){ uint32_t px=g_pointer_x+x, py=g_pointer_y+y; if(px>=wl_fb_w||py>=wl_fb_h) continue; if(x<=y) wl_fb[py*wl_fb_w+px]=0xFFFFFF; } }

static void present_surface(uint32_t sid){
    if(sid>=WL_OBJECT_MAX) return;
    wl_obj_state* s=&g_objs[sid]; if(s->type!=O_WL_SURFACE||!s->attached_buffer_id) return;
    wl_obj_state* b=&g_objs[s->attached_buffer_id]; if(b->type!=O_WL_BUFFER||!b->pool_id||b->pool_id>=WL_OBJECT_MAX) return;
    wl_obj_state* p=&g_objs[b->pool_id]; if(p->type!=O_WL_SHM_POOL||!p->map) return;
    draw_shell_background();
    uint32_t cw=(b->width>0&&(uint32_t)b->width<wl_fb_w)?(uint32_t)b->width:wl_fb_w;
    uint32_t ch=(b->height>0&&(uint32_t)b->height<wl_fb_h)?(uint32_t)b->height:wl_fb_h;
    for(uint32_t y=0;y<ch;++y){ uint32_t off=b->offset+y*(uint32_t)b->stride; if(off+cw*4U>p->size) break; memcpy(&wl_fb[y*wl_fb_w], p->map+off, cw*4U); }
    draw_pointer();
    (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0);
    if(s->frame_callback_id && s->frame_callback_id<WL_OBJECT_MAX){ send_callback_done((int)s->client_fd,s->frame_callback_id); g_objs[s->frame_callback_id].type=0; s->frame_callback_id=0; }
    send_buffer_release((int)s->client_fd, s->attached_buffer_id);
}

static void clear_objects(int fd){ memset(g_objs,0,sizeof(g_objs)); memset(&g_client,0,sizeof(g_client)); g_client.fd=fd; g_objs[1].type=O_WL_DISPLAY; g_objs[2].type=O_WL_COMPOSITOR; g_objs[3].type=O_WL_SHM; }
static void send_output_info(int fd, uint32_t oid){ uint8_t pkt[128]={0}; uint32_t at=0; at=append_u32(pkt,at,oid); uint32_t hdr=at; at=append_u32(pkt,at,0); at=append_i32(pkt,at,0); at=append_i32(pkt,at,0); at=append_i32(pkt,at,300); at=append_i32(pkt,at,170); at=append_u32(pkt,at,1); at=append_string(pkt,at,"AMS"); at=append_string(pkt,at,"Virtual-0"); at=append_i32(pkt,at,0); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1); uint8_t mode[24]={0}; send_event_header(mode,oid,1,20); wr_u32(mode+8,3); wr_u32(mode+12,wl_fb_w); wr_u32(mode+16,wl_fb_h); wr_u32(mode+20,60000); (void)send_packet(fd,mode,24,-1); uint8_t scale[12]={0}; send_event_header(scale,oid,3,12); wr_u32(scale+8,1); (void)send_packet(fd,scale,12,-1); uint8_t done[8]={0}; send_event_header(done,oid,2,8); (void)send_packet(fd,done,8,-1); }
static void send_seat_info(int fd, uint32_t sid){ uint8_t caps[12]={0}; send_event_header(caps,sid,0,12); wr_u32(caps+8,3); (void)send_packet(fd,caps,12,-1); uint8_t name[64]={0}; uint32_t at=0; at=append_u32(name,at,sid); uint32_t hdr=at; at=append_u32(name,at,0); at=append_string(name,at,"seat0"); wr_u32(name+hdr,(at<<16)|1); (void)send_packet(fd,name,at,-1); }
static void send_pointer_enter(int fd, uint32_t pid, uint32_t sid){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,0,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,g_pointer_x<<8); wr_u32(pkt+20,g_pointer_y<<8); (void)send_packet(fd,pkt,24,-1); }
static void send_pointer_motion(int fd, uint32_t pid){ uint8_t pkt[20]={0}; send_event_header(pkt,pid,2,20); wr_u32(pkt+8,now_ms()); wr_u32(pkt+12,g_pointer_x<<8); wr_u32(pkt+16,g_pointer_y<<8); (void)send_packet(fd,pkt,20,-1); }
static void send_pointer_button(int fd,uint32_t pid,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,0x110); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }
static void send_keyboard_enter(int fd,uint32_t kid,uint32_t sid){ uint8_t pkt[20]={0}; send_event_header(pkt,kid,1,20); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,0); (void)send_packet(fd,pkt,20,-1); }
static void send_keyboard_key(int fd,uint32_t kid,uint32_t key,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,kid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,key); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }

static void handle_input(void){
    uint64_t mev=ams_syscall(SYS_AMS_GET_MOUSE_EVENT,0,0,0,0,0);
    if(mev && g_client.pointer_id && g_client.focused_surface){ g_pointer_x=(uint32_t)(mev&0xFFFFU); g_pointer_y=(uint32_t)((mev>>16)&0xFFFFU); uint8_t buttons=(uint8_t)((mev>>32)&0xFFU); uint8_t old=g_pointer_buttons; g_pointer_buttons=buttons; send_pointer_motion(g_client.fd,g_client.pointer_id); if(old!=g_pointer_buttons) send_pointer_button(g_client.fd,g_client.pointer_id,(g_pointer_buttons&1U)?1U:0U); }
    uint64_t kev=ams_syscall(SYS_AMS_GET_KEY,0,0,0,0,0);
    if(kev && g_client.keyboard_id && g_client.focused_surface){ int32_t k=(int32_t)kev; uint32_t st=1; if(k<0){st=0;k=-k;} send_keyboard_key(g_client.fd,g_client.keyboard_id,(uint32_t)k,st); }
}

static void process_message(int fd, wl_fd_queue* fdq, uint32_t oid, uint16_t op, const uint8_t* p, uint32_t n){
    if(oid>=WL_OBJECT_MAX) return;
    wl_obj_state* o=&g_objs[oid];
    if(oid==1 && o->type==O_WL_DISPLAY){ if(op==0 && n>=4){ uint32_t cb=rd_u32(p); if(cb&&cb<WL_OBJECT_MAX){ g_objs[cb].type=O_WL_CALLBACK; send_callback_done(fd,cb);} } if(op==1 && n>=4){ uint32_t rid=rd_u32(p); if(rid&&rid<WL_OBJECT_MAX){ g_objs[rid].type=O_WL_REGISTRY; send_registry_global(fd,rid,1,"wl_compositor",4); send_registry_global(fd,rid,2,"wl_shm",1); send_registry_global(fd,rid,3,"wl_output",2); send_registry_global(fd,rid,4,"wl_seat",5); send_registry_global(fd,rid,5,"xdg_wm_base",1);} } return; }
    if(o->type==O_WL_REGISTRY){ if(op==0 && n>=16){ uint32_t name=rd_u32(p); uint32_t sl=rd_u32(p+4); uint32_t sp=(sl+3U)&~3U; if(n<4+4+sp+8) return; uint32_t nid=rd_u32(p+12+sp); if(!nid||nid>=WL_OBJECT_MAX) return; if(name==1) g_objs[nid].type=O_WL_COMPOSITOR; else if(name==2){ g_objs[nid].type=O_WL_SHM; uint8_t f[12]={0}; send_event_header(f,nid,0,12); wr_u32(f+8,0); (void)send_packet(fd,f,12,-1); } else if(name==3){ g_objs[nid].type=O_WL_OUTPUT; send_output_info(fd,nid); } else if(name==4){ g_objs[nid].type=O_WL_SEAT; send_seat_info(fd,nid); } else if(name==5){ g_objs[nid].type=O_XDG_WM_BASE; uint8_t ping[12]={0}; send_event_header(ping,nid,0,12); wr_u32(ping+8,++g_client.serial); (void)send_packet(fd,ping,12,-1); } } return; }
    if(o->type==O_WL_COMPOSITOR){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_SURFACE; g_objs[nid].client_fd=(uint32_t)fd; } } return; }
    if(o->type==O_WL_SHM){ if(op==0&&n>=8){ uint32_t nid=rd_u32(p), sz=rd_u32(p+4); int passed=fdq_pop(fdq); if(!nid||nid>=WL_OBJECT_MAX||passed<0||sz==0) return; g_objs[nid].type=O_WL_SHM_POOL; g_objs[nid].fd=passed; g_objs[nid].size=sz; g_objs[nid].map=(uint8_t*)mmap(0,sz,PROT_READ,MAP_SHARED,passed,0); if((uint64_t)g_objs[nid].map>(uint64_t)-4096LL) g_objs[nid].map=0;} return; }
    if(o->type==O_WL_SHM_POOL){ if(op==0&&n>=24){ uint32_t nid=rd_u32(p); if(!nid||nid>=WL_OBJECT_MAX) return; g_objs[nid].type=O_WL_BUFFER; g_objs[nid].pool_id=oid; g_objs[nid].offset=rd_u32(p+4); g_objs[nid].width=(int32_t)rd_u32(p+8); g_objs[nid].height=(int32_t)rd_u32(p+12); g_objs[nid].stride=(int32_t)rd_u32(p+16); g_objs[nid].format=rd_u32(p+20); g_objs[nid].client_fd=(uint32_t)fd; } return; }
    if(o->type==O_WL_SURFACE){ if(op==1&&n>=12) o->attached_buffer_id=rd_u32(p); else if(op==3&&n>=4){ uint32_t cb=rd_u32(p); o->frame_callback_id=cb; if(cb&&cb<WL_OBJECT_MAX) g_objs[cb].type=O_WL_CALLBACK; } else if(op==6){ g_client.focused_surface=oid; present_surface(oid); if(g_client.pointer_id) send_pointer_enter(fd,g_client.pointer_id,oid); if(g_client.keyboard_id) send_keyboard_enter(fd,g_client.keyboard_id,oid); } return; }
    if(o->type==O_WL_SEAT){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_POINTER; g_client.pointer_id=nid; }} else if(op==1&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_KEYBOARD; g_client.keyboard_id=nid; }} return; }
    if(o->type==O_XDG_WM_BASE){ if(op==1&&n>=8){ uint32_t xs=rd_u32(p), sid=rd_u32(p+4); if(xs&&xs<WL_OBJECT_MAX&&sid&&sid<WL_OBJECT_MAX){ g_objs[xs].type=O_XDG_SURFACE; g_objs[xs].role_id=sid; g_objs[sid].role_id=xs; uint8_t cfg[12]={0}; send_event_header(cfg,xs,0,12); wr_u32(cfg+8,++g_client.serial); (void)send_packet(fd,cfg,12,-1); }} return; }
    if(o->type==O_XDG_SURFACE){ if(op==1&&n>=4){ uint32_t tl=rd_u32(p); if(tl&&tl<WL_OBJECT_MAX){ g_objs[tl].type=O_XDG_TOPLEVEL; uint8_t tcfg[20]={0}; send_event_header(tcfg,tl,0,20); wr_u32(tcfg+8,wl_fb_w); wr_u32(tcfg+12,wl_fb_h); wr_u32(tcfg+16,0); (void)send_packet(fd,tcfg,20,-1); uint8_t scfg[12]={0}; send_event_header(scfg,oid,0,12); wr_u32(scfg+8,++g_client.serial); (void)send_packet(fd,scfg,12,-1); }} return; }
    if(o->type==O_WL_BUFFER && op==0) o->type=0;
}

static int bootstrap_local_shell(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int fd=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(fd<0) return -1;
    if((int)ams_syscall(SYS_CONNECT,(uint64_t)fd,(uint64_t)&addr,sizeof(addr),0,0)<0) return -2;
    const uint32_t reg=40, comp=41, shm=42, surf=43, pool=44, buf=45, xwm=46, xsurf=47, xtop=48;
    uint8_t m[512]={0}; uint32_t at=0, h=0;
    at=append_u32(m,at,1); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,reg); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,1); at=append_string(m,at,"wl_compositor"); at=append_u32(m,at,4); at=append_u32(m,at,comp); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,2); at=append_string(m,at,"wl_shm"); at=append_u32(m,at,1); at=append_u32(m,at,shm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,5); at=append_string(m,at,"xdg_wm_base"); at=append_u32(m,at,1); at=append_u32(m,at,xwm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,comp); at=append_u32(m,at,(12U<<16)|0U); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xwm); at=append_u32(m,at,(16U<<16)|1U); at=append_u32(m,at,xsurf); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xsurf); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,xtop); (void)send_packet(fd,m,at,-1);
    int w=960,hh=540,stride=w*4,size=stride*hh; int shmfd=(int)ams_syscall(SYS_MEMFD_CREATE,(uint64_t)"wl-shell",0,0,0,0); if(shmfd<0) return -3;
    if((int)ams_syscall(SYS_FTRUNCATE,(uint64_t)shmfd,(uint64_t)size,0,0,0)<0) return -4;
    uint32_t* pix=(uint32_t*)mmap(0,(size_t)size,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0); if((uint64_t)pix>(uint64_t)-4096LL) return -5;
    for(int y=0;y<hh;++y) for(int x=0;x<w;++x) pix[y*w+x]=(y<40)?0x2B394C:0x1A2230;
    at=0; at=append_u32(m,at,shm); at=append_u32(m,at,(16U<<16)|0U); at=append_u32(m,at,pool); at=append_u32(m,at,(uint32_t)size); (void)send_packet(fd,m,at,shmfd);
    at=0; at=append_u32(m,at,pool); at=append_u32(m,at,(32U<<16)|0U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,(uint32_t)w); at=append_u32(m,at,(uint32_t)hh); at=append_u32(m,at,(uint32_t)stride); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(20U<<16)|1U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(8U<<16)|6U); (void)send_packet(fd,m,at,-1);
    return fd;
}

static void handle_client(int cli){
    uint8_t rx[WL_RX_CAP]; uint32_t rx_len=0; wl_fd_queue fdq; fdq_init(&fdq); clear_objects(cli);
    puts1("wl-compositor: client connected");
    while(1){ int pass=-1; int n=recv_packet(cli,rx+rx_len,WL_RX_CAP-rx_len,&pass); if(n==0) break; if(n<0){ handle_input(); continue; } if(pass>=0) (void)fdq_push(&fdq,pass); rx_len+=(uint32_t)n; uint32_t at=0; while(rx_len-at>=8){ uint32_t oid=rd_u32(rx+at), hdr=rd_u32(rx+at+4); uint16_t op=(uint16_t)(hdr&0xFFFFU), sz=(uint16_t)(hdr>>16); if(sz<8||at+sz>rx_len) break; process_message(cli,&fdq,oid,op,rx+at+8,(uint32_t)sz-8); at+=sz; } if(at>0){ memmove(rx,rx+at,rx_len-at); rx_len-=at; } handle_input(); }
    puts1("wl-compositor: client disconnected");
}

int main(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int srv=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(srv<0){ puts1("wl-compositor: socket failed"); return 1; }
    if((int)ams_syscall(SYS_BIND,srv,(uint64_t)&addr,sizeof(addr),0,0)<0){ puts1("wl-compositor: bind failed"); return 2; }
    (void)ams_syscall(SYS_LISTEN,srv,8,0,0,0); puts1("wl-compositor: listening on wayland-0");
    if((int)ams_syscall(SYS_AMS_GET_FB_INFO,(uint64_t)&wl_fb_w,(uint64_t)&wl_fb_h,0,0,0)!=0||wl_fb_w==0||wl_fb_h==0){ wl_fb_w=1280; wl_fb_h=720; }
    wl_fb=(uint32_t*)malloc((size_t)wl_fb_w*(size_t)wl_fb_h*sizeof(uint32_t)); if(!wl_fb){ puts1("wl-compositor: fb buffer alloc failed"); return 3; }
    draw_shell_background(); draw_pointer(); (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0);
    puts1("wl-compositor: protocol core+xdg+seat ready");
    if(bootstrap_local_shell()>=0) puts1("wl-compositor: local shell bootstrap queued"); else puts1("wl-compositor: local shell bootstrap failed");
    while(1){ int cli=(int)ams_syscall(SYS_ACCEPT,srv,0,0,0,0); if(cli<0){ handle_input(); continue; } handle_client(cli); }
}
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01

#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32

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
struct linux_msghdr {
    void* msg_name; uint32_t msg_namelen; uint32_t __pad0;
    struct linux_iovec* msg_iov; uint64_t msg_iovlen;
    void* msg_control; uint64_t msg_controllen; uint32_t msg_flags; uint32_t __pad1;
};
struct linux_cmsghdr { uint64_t cmsg_len; int32_t cmsg_level; int32_t cmsg_type; };
struct linux_timespec_local { int64_t tv_sec; int64_t tv_nsec; };

typedef struct wl_obj_state {
    uint32_t type, pool_id, offset, format, attached_buffer_id, role_id, frame_callback_id, client_fd;
    int fd;
    uint8_t* map;
    uint32_t size;
    int32_t width, height, stride;
} wl_obj_state;

typedef struct wl_fd_queue { int data[WL_FDQ_CAP]; uint32_t head, tail; } wl_fd_queue;
typedef struct wl_client_state {
    int fd;
    uint32_t pointer_id, keyboard_id, focused_surface, serial;
} wl_client_state;

static wl_obj_state g_objs[WL_OBJECT_MAX];
static wl_client_state g_client = {0};
static uint32_t* wl_fb = 0;
static uint32_t wl_fb_w = 1280, wl_fb_h = 720;
static uint32_t g_pointer_x = 80, g_pointer_y = 80;
static uint8_t g_pointer_buttons = 0;

static void puts1(const char* s) { int n = 0; while (s[n]) ++n; ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0); ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0); }
static uint32_t now_ms(void) { struct linux_timespec_local ts; if ((long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0) != 0) return 0; return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
static uint32_t rd_u32(const uint8_t* p) { return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr_u32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)(v&0xFF); p[1]=(uint8_t)((v>>8)&0xFF); p[2]=(uint8_t)((v>>16)&0xFF); p[3]=(uint8_t)((v>>24)&0xFF); }
static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) { wr_u32(out + at, v); return at + 4; }
static uint32_t append_i32(uint8_t* out, uint32_t at, int32_t v) { wr_u32(out + at, (uint32_t)v); return at + 4; }
static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) { uint32_t len=0; while (s[len]) ++len; at=append_u32(out,at,len+1); memcpy(out+at,s,len); out[at+len]=0; at+=len+1; while(at&3U) out[at++]=0; return at; }

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0}; struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int); ch->cmsg_level = SOL_SOCKET; ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd; msg.msg_control = control; msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) {
    struct linux_iovec iov = {data, cap};
    uint8_t control[64] = {0}; struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1; msg.msg_control = control; msg.msg_controllen = sizeof(control); *recv_fd = -1;
    int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
    if (rc <= 0) return rc;
    if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS) *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr));
    }
    return rc;
}

static void fdq_init(wl_fd_queue* q){ q->head=0; q->tail=0; for(uint32_t i=0;i<WL_FDQ_CAP;++i) q->data[i]=-1; }
static int fdq_push(wl_fd_queue* q,int fd){ uint32_t n=(q->tail+1U)%WL_FDQ_CAP; if(n==q->head) return -1; q->data[q->tail]=fd; q->tail=n; return 0; }
static int fdq_pop(wl_fd_queue* q){ if(q->head==q->tail) return -1; int fd=q->data[q->head]; q->head=(q->head+1U)%WL_FDQ_CAP; return fd; }

static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size){ wr_u32(pkt, obj_id); wr_u32(pkt+4, ((uint32_t)size<<16)|opcode); }
static void send_callback_done(int fd, uint32_t cb_id){ uint8_t pkt[12]={0}; send_event_header(pkt, cb_id, 0, 12); wr_u32(pkt+8, now_ms()); (void)send_packet(fd,pkt,12,-1); }
static void send_buffer_release(int fd, uint32_t id){ uint8_t pkt[8]={0}; send_event_header(pkt,id,0,8); (void)send_packet(fd,pkt,8,-1); }

static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) {
    uint8_t pkt[256]={0}; uint32_t at=0; at=append_u32(pkt,at,reg_id); uint32_t hdr=at; at=append_u32(pkt,at,0); at=append_u32(pkt,at,name); at=append_string(pkt,at,iface); at=append_u32(pkt,at,version); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1);
}

static void draw_shell_background(void){
    if(!wl_fb) return;
    for(uint32_t y=0;y<wl_fb_h;++y){ uint32_t c=(y<36)?0x1E2733:0x151C26; for(uint32_t x=0;x<wl_fb_w;++x) wl_fb[y*wl_fb_w+x]=c; }
}
static void draw_pointer(void){
    for(uint32_t y=0;y<12;++y) for(uint32_t x=0;x<10;++x){ uint32_t px=g_pointer_x+x, py=g_pointer_y+y; if(px>=wl_fb_w||py>=wl_fb_h) continue; if(x<=y) wl_fb[py*wl_fb_w+px]=0xFFFFFF; }
}

static void present_surface(uint32_t sid){
    if(sid>=WL_OBJECT_MAX) return;
    wl_obj_state* s=&g_objs[sid]; if(s->type!=O_WL_SURFACE||!s->attached_buffer_id) return;
    wl_obj_state* b=&g_objs[s->attached_buffer_id]; if(b->type!=O_WL_BUFFER||!b->pool_id||b->pool_id>=WL_OBJECT_MAX) return;
    wl_obj_state* p=&g_objs[b->pool_id]; if(p->type!=O_WL_SHM_POOL||!p->map) return;
    draw_shell_background();
    uint32_t cw=(b->width>0&&(uint32_t)b->width<wl_fb_w)?(uint32_t)b->width:wl_fb_w;
    uint32_t ch=(b->height>0&&(uint32_t)b->height<wl_fb_h)?(uint32_t)b->height:wl_fb_h;
    for(uint32_t y=0;y<ch;++y){ uint32_t off=b->offset+y*(uint32_t)b->stride; if(off+cw*4U>p->size) break; memcpy(&wl_fb[y*wl_fb_w], p->map+off, cw*4U); }
    draw_pointer();
    (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0);
    if(s->frame_callback_id && s->frame_callback_id<WL_OBJECT_MAX){ send_callback_done((int)s->client_fd,s->frame_callback_id); g_objs[s->frame_callback_id].type=0; s->frame_callback_id=0; }
    send_buffer_release((int)s->client_fd, s->attached_buffer_id);
}

static void clear_objects(int fd){
    memset(g_objs,0,sizeof(g_objs)); memset(&g_client,0,sizeof(g_client)); g_client.fd=fd;
    g_objs[1].type=O_WL_DISPLAY; g_objs[2].type=O_WL_COMPOSITOR; g_objs[3].type=O_WL_SHM;
}

static void send_output_info(int fd, uint32_t oid){
    uint8_t pkt[128]={0}; uint32_t at=0; at=append_u32(pkt,at,oid); uint32_t hdr=at; at=append_u32(pkt,at,0);
    at=append_i32(pkt,at,0); at=append_i32(pkt,at,0); at=append_i32(pkt,at,300); at=append_i32(pkt,at,170); at=append_u32(pkt,at,1); at=append_string(pkt,at,"AMS"); at=append_string(pkt,at,"Virtual-0"); at=append_i32(pkt,at,0); wr_u32(pkt+hdr,(at<<16)|0); (void)send_packet(fd,pkt,at,-1);
    uint8_t mode[24]={0}; send_event_header(mode,oid,1,20); wr_u32(mode+8,3); wr_u32(mode+12,wl_fb_w); wr_u32(mode+16,wl_fb_h); wr_u32(mode+20,60000); (void)send_packet(fd,mode,24,-1);
    uint8_t scale[12]={0}; send_event_header(scale,oid,3,12); wr_u32(scale+8,1); (void)send_packet(fd,scale,12,-1);
    uint8_t done[8]={0}; send_event_header(done,oid,2,8); (void)send_packet(fd,done,8,-1);
}

static void send_seat_info(int fd, uint32_t sid){
    uint8_t caps[12]={0}; send_event_header(caps,sid,0,12); wr_u32(caps+8,3); (void)send_packet(fd,caps,12,-1);
    uint8_t name[64]={0}; uint32_t at=0; at=append_u32(name,at,sid); uint32_t hdr=at; at=append_u32(name,at,0); at=append_string(name,at,"seat0"); wr_u32(name+hdr,(at<<16)|1); (void)send_packet(fd,name,at,-1);
}

static void send_pointer_enter(int fd, uint32_t pid, uint32_t sid){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,0,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,g_pointer_x<<8); wr_u32(pkt+20,g_pointer_y<<8); (void)send_packet(fd,pkt,24,-1); }
static void send_pointer_motion(int fd, uint32_t pid){ uint8_t pkt[20]={0}; send_event_header(pkt,pid,2,20); wr_u32(pkt+8,now_ms()); wr_u32(pkt+12,g_pointer_x<<8); wr_u32(pkt+16,g_pointer_y<<8); (void)send_packet(fd,pkt,20,-1); }
static void send_pointer_button(int fd,uint32_t pid,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,pid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,0x110); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }
static void send_keyboard_enter(int fd,uint32_t kid,uint32_t sid){ uint8_t pkt[20]={0}; send_event_header(pkt,kid,1,20); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,sid); wr_u32(pkt+16,0); (void)send_packet(fd,pkt,20,-1); }
static void send_keyboard_key(int fd,uint32_t kid,uint32_t key,uint32_t state){ uint8_t pkt[24]={0}; send_event_header(pkt,kid,3,24); wr_u32(pkt+8,++g_client.serial); wr_u32(pkt+12,now_ms()); wr_u32(pkt+16,key); wr_u32(pkt+20,state); (void)send_packet(fd,pkt,24,-1); }

static void handle_input(void){
    uint64_t mev=ams_syscall(SYS_AMS_GET_MOUSE_EVENT,0,0,0,0,0);
    if(mev && g_client.pointer_id && g_client.focused_surface){
        g_pointer_x=(uint32_t)(mev&0xFFFFU); g_pointer_y=(uint32_t)((mev>>16)&0xFFFFU);
        uint8_t buttons=(uint8_t)((mev>>32)&0xFFU); uint8_t old=g_pointer_buttons; g_pointer_buttons=buttons;
        send_pointer_motion(g_client.fd,g_client.pointer_id);
        if(old!=g_pointer_buttons) send_pointer_button(g_client.fd,g_client.pointer_id,(g_pointer_buttons&1U)?1U:0U);
    }
    uint64_t kev=ams_syscall(SYS_AMS_GET_KEY,0,0,0,0,0);
    if(kev && g_client.keyboard_id && g_client.focused_surface){
        int32_t k=(int32_t)kev; uint32_t st=1; if(k<0){st=0;k=-k;} send_keyboard_key(g_client.fd,g_client.keyboard_id,(uint32_t)k,st);
    }
}

static void process_message(int fd, wl_fd_queue* fdq, uint32_t oid, uint16_t op, const uint8_t* p, uint32_t n){
    if(oid>=WL_OBJECT_MAX) return;
    wl_obj_state* o=&g_objs[oid];
    if(oid==1 && o->type==O_WL_DISPLAY){
        if(op==0 && n>=4){ uint32_t cb=rd_u32(p); if(cb&&cb<WL_OBJECT_MAX){ g_objs[cb].type=O_WL_CALLBACK; send_callback_done(fd,cb);} }
        if(op==1 && n>=4){ uint32_t rid=rd_u32(p); if(rid&&rid<WL_OBJECT_MAX){ g_objs[rid].type=O_WL_REGISTRY; send_registry_global(fd,rid,1,"wl_compositor",4); send_registry_global(fd,rid,2,"wl_shm",1); send_registry_global(fd,rid,3,"wl_output",2); send_registry_global(fd,rid,4,"wl_seat",5); send_registry_global(fd,rid,5,"xdg_wm_base",1);} }
        return;
    }
    if(o->type==O_WL_REGISTRY){
        if(op==0 && n>=16){ uint32_t name=rd_u32(p); uint32_t sl=rd_u32(p+4); uint32_t sp=(sl+3U)&~3U; if(n<4+4+sp+8) return; uint32_t nid=rd_u32(p+12+sp); if(!nid||nid>=WL_OBJECT_MAX) return;
            if(name==1) g_objs[nid].type=O_WL_COMPOSITOR;
            else if(name==2){ g_objs[nid].type=O_WL_SHM; uint8_t f[12]={0}; send_event_header(f,nid,0,12); wr_u32(f+8,0); (void)send_packet(fd,f,12,-1); }
            else if(name==3){ g_objs[nid].type=O_WL_OUTPUT; send_output_info(fd,nid); }
            else if(name==4){ g_objs[nid].type=O_WL_SEAT; send_seat_info(fd,nid); }
            else if(name==5){ g_objs[nid].type=O_XDG_WM_BASE; uint8_t ping[12]={0}; send_event_header(ping,nid,0,12); wr_u32(ping+8,++g_client.serial); (void)send_packet(fd,ping,12,-1); }
        } return;
    }
    if(o->type==O_WL_COMPOSITOR){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_SURFACE; g_objs[nid].client_fd=(uint32_t)fd; } } return; }
    if(o->type==O_WL_SHM){ if(op==0&&n>=8){ uint32_t nid=rd_u32(p), sz=rd_u32(p+4); int passed=fdq_pop(fdq); if(!nid||nid>=WL_OBJECT_MAX||passed<0||sz==0) return; g_objs[nid].type=O_WL_SHM_POOL; g_objs[nid].fd=passed; g_objs[nid].size=sz; g_objs[nid].map=(uint8_t*)mmap(0,sz,PROT_READ,MAP_SHARED,passed,0); if((uint64_t)g_objs[nid].map>(uint64_t)-4096LL) g_objs[nid].map=0;} return; }
    if(o->type==O_WL_SHM_POOL){ if(op==0&&n>=24){ uint32_t nid=rd_u32(p); if(!nid||nid>=WL_OBJECT_MAX) return; g_objs[nid].type=O_WL_BUFFER; g_objs[nid].pool_id=oid; g_objs[nid].offset=rd_u32(p+4); g_objs[nid].width=(int32_t)rd_u32(p+8); g_objs[nid].height=(int32_t)rd_u32(p+12); g_objs[nid].stride=(int32_t)rd_u32(p+16); g_objs[nid].format=rd_u32(p+20); g_objs[nid].client_fd=(uint32_t)fd; } return; }
    if(o->type==O_WL_SURFACE){
        if(op==1&&n>=12) o->attached_buffer_id=rd_u32(p);
        else if(op==3&&n>=4){ uint32_t cb=rd_u32(p); o->frame_callback_id=cb; if(cb&&cb<WL_OBJECT_MAX) g_objs[cb].type=O_WL_CALLBACK; }
        else if(op==6){ g_client.focused_surface=oid; present_surface(oid); if(g_client.pointer_id) send_pointer_enter(fd,g_client.pointer_id,oid); if(g_client.keyboard_id) send_keyboard_enter(fd,g_client.keyboard_id,oid); }
        return;
    }
    if(o->type==O_WL_SEAT){ if(op==0&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_POINTER; g_client.pointer_id=nid; }} else if(op==1&&n>=4){ uint32_t nid=rd_u32(p); if(nid&&nid<WL_OBJECT_MAX){ g_objs[nid].type=O_WL_KEYBOARD; g_client.keyboard_id=nid; }} return; }
    if(o->type==O_XDG_WM_BASE){ if(op==1&&n>=8){ uint32_t xs=rd_u32(p), sid=rd_u32(p+4); if(xs&&xs<WL_OBJECT_MAX&&sid&&sid<WL_OBJECT_MAX){ g_objs[xs].type=O_XDG_SURFACE; g_objs[xs].role_id=sid; g_objs[sid].role_id=xs; uint8_t cfg[12]={0}; send_event_header(cfg,xs,0,12); wr_u32(cfg+8,++g_client.serial); (void)send_packet(fd,cfg,12,-1); }} return; }
    if(o->type==O_XDG_SURFACE){ if(op==1&&n>=4){ uint32_t tl=rd_u32(p); if(tl&&tl<WL_OBJECT_MAX){ g_objs[tl].type=O_XDG_TOPLEVEL; uint8_t tcfg[20]={0}; send_event_header(tcfg,tl,0,20); wr_u32(tcfg+8,wl_fb_w); wr_u32(tcfg+12,wl_fb_h); wr_u32(tcfg+16,0); (void)send_packet(fd,tcfg,20,-1); uint8_t scfg[12]={0}; send_event_header(scfg,oid,0,12); wr_u32(scfg+8,++g_client.serial); (void)send_packet(fd,scfg,12,-1); }} return; }
    if(o->type==O_WL_BUFFER && op==0) o->type=0;
}

static int bootstrap_local_shell(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int fd=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(fd<0) return -1;
    if((int)ams_syscall(SYS_CONNECT,(uint64_t)fd,(uint64_t)&addr,sizeof(addr),0,0)<0) return -2;
    const uint32_t reg=40, comp=41, shm=42, surf=43, pool=44, buf=45, xwm=46, xsurf=47, xtop=48;
    uint8_t m[512]={0}; uint32_t at=0, h=0;
    at=append_u32(m,at,1); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,reg); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,1); at=append_string(m,at,"wl_compositor"); at=append_u32(m,at,4); at=append_u32(m,at,comp); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,2); at=append_string(m,at,"wl_shm"); at=append_u32(m,at,1); at=append_u32(m,at,shm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,reg); h=at; at=append_u32(m,at,0); at=append_u32(m,at,5); at=append_string(m,at,"xdg_wm_base"); at=append_u32(m,at,1); at=append_u32(m,at,xwm); wr_u32(m+h,(at<<16)|0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,comp); at=append_u32(m,at,(12U<<16)|0U); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xwm); at=append_u32(m,at,(16U<<16)|1U); at=append_u32(m,at,xsurf); at=append_u32(m,at,surf); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,xsurf); at=append_u32(m,at,(12U<<16)|1U); at=append_u32(m,at,xtop); (void)send_packet(fd,m,at,-1);
    int w=960,hh=540,stride=w*4,size=stride*hh; int shmfd=(int)ams_syscall(SYS_MEMFD_CREATE,(uint64_t)"wl-shell",0,0,0,0); if(shmfd<0) return -3;
    if((int)ams_syscall(SYS_FTRUNCATE,(uint64_t)shmfd,(uint64_t)size,0,0,0)<0) return -4;
    uint32_t* pix=(uint32_t*)mmap(0,(size_t)size,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0); if((uint64_t)pix>(uint64_t)-4096LL) return -5;
    for(int y=0;y<hh;++y) for(int x=0;x<w;++x) pix[y*w+x]=(y<40)?0x2B394C:0x1A2230;
    at=0; at=append_u32(m,at,shm); at=append_u32(m,at,(16U<<16)|0U); at=append_u32(m,at,pool); at=append_u32(m,at,(uint32_t)size); (void)send_packet(fd,m,at,shmfd);
    at=0; at=append_u32(m,at,pool); at=append_u32(m,at,(32U<<16)|0U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,(uint32_t)w); at=append_u32(m,at,(uint32_t)hh); at=append_u32(m,at,(uint32_t)stride); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(20U<<16)|1U); at=append_u32(m,at,buf); at=append_u32(m,at,0); at=append_u32(m,at,0); (void)send_packet(fd,m,at,-1);
    at=0; at=append_u32(m,at,surf); at=append_u32(m,at,(8U<<16)|6U); (void)send_packet(fd,m,at,-1);
    return fd;
}

static void handle_client(int cli){
    uint8_t rx[WL_RX_CAP]; uint32_t rx_len=0; wl_fd_queue fdq; fdq_init(&fdq); clear_objects(cli);
    puts1("wl-compositor: client connected");
    while(1){
        int pass=-1; int n=recv_packet(cli,rx+rx_len,WL_RX_CAP-rx_len,&pass);
        if(n==0) break;
        if(n<0){ handle_input(); continue; }
        if(pass>=0) (void)fdq_push(&fdq,pass);
        rx_len+=(uint32_t)n;
        uint32_t at=0; while(rx_len-at>=8){ uint32_t oid=rd_u32(rx+at), hdr=rd_u32(rx+at+4); uint16_t op=(uint16_t)(hdr&0xFFFFU), sz=(uint16_t)(hdr>>16); if(sz<8||at+sz>rx_len) break; process_message(cli,&fdq,oid,op,rx+at+8,(uint32_t)sz-8); at+=sz; }
        if(at>0){ memmove(rx,rx+at,rx_len-at); rx_len-=at; }
        handle_input();
    }
    puts1("wl-compositor: client disconnected");
}

int main(void){
    struct linux_sockaddr_un addr={0}; addr.sun_family=AF_UNIX; { const char* p="/run/user/0/wayland-0"; for(int i=0;p[i]&&i<107;++i) addr.sun_path[i]=p[i]; }
    int srv=(int)ams_syscall(SYS_SOCKET,AF_UNIX,SOCK_STREAM,0,0,0); if(srv<0){ puts1("wl-compositor: socket failed"); return 1; }
    if((int)ams_syscall(SYS_BIND,srv,(uint64_t)&addr,sizeof(addr),0,0)<0){ puts1("wl-compositor: bind failed"); return 2; }
    (void)ams_syscall(SYS_LISTEN,srv,8,0,0,0); puts1("wl-compositor: listening on wayland-0");
    if((int)ams_syscall(SYS_AMS_GET_FB_INFO,(uint64_t)&wl_fb_w,(uint64_t)&wl_fb_h,0,0,0)!=0||wl_fb_w==0||wl_fb_h==0){ wl_fb_w=1280; wl_fb_h=720; }
    wl_fb=(uint32_t*)malloc((size_t)wl_fb_w*(size_t)wl_fb_h*sizeof(uint32_t)); if(!wl_fb){ puts1("wl-compositor: fb buffer alloc failed"); return 3; }
    draw_shell_background(); draw_pointer(); (void)ams_syscall(SYS_AMS_FB_BLIT,(uint64_t)wl_fb,wl_fb_w,wl_fb_h,0,0);
    puts1("wl-compositor: protocol core+xdg+seat ready");
    if(bootstrap_local_shell()>=0) puts1("wl-compositor: local shell bootstrap queued"); else puts1("wl-compositor: local shell bootstrap failed");
    while(1){ int cli=(int)ams_syscall(SYS_ACCEPT,srv,0,0,0,0); if(cli<0){ handle_input(); continue; } handle_client(cli); }
}
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43
#define SYS_CONNECT 42
#define SYS_SENDMSG 46
#define SYS_RECVMSG 47
#define SYS_CLOSE 3
#define SYS_MEMFD_CREATE 319
#define SYS_FTRUNCATE 77
#define SYS_CLOCK_GETTIME 228

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01

#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32

#define WL_DISPLAY 1
#define WL_REGISTRY 2
#define WL_COMPOSITOR 3
#define WL_SHM 4
#define WL_SURFACE 5
#define WL_SHM_POOL 6
#define WL_BUFFER 7
#define WL_CALLBACK 8
#define WL_OUTPUT 9
#define WL_SEAT 10
#define WL_POINTER 11
#define WL_KEYBOARD 12
#define XDG_WM_BASE 20
#define XDG_SURFACE 21
#define XDG_TOPLEVEL 22

#define WL_SHM_FORMAT_XRGB8888 0
#define WL_SEAT_CAP_POINTER 2
#define WL_SEAT_CAP_KEYBOARD 1

struct linux_sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

struct linux_iovec {
    void* iov_base;
    uint64_t iov_len;
};

struct linux_msghdr {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    struct linux_iovec* msg_iov;
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

struct linux_timespec_local {
    int64_t tv_sec;
    int64_t tv_nsec;
};

typedef struct wl_obj_state {
    uint32_t type;
    int fd;
    uint8_t* map;
    uint32_t size;
    uint32_t pool_id;
    uint32_t offset;
    int32_t width;
    int32_t height;
    int32_t stride;
    uint32_t format;
    uint32_t attached_buffer_id;
    uint32_t role_id;
    uint32_t frame_callback_id;
    uint32_t client_fd;
} wl_obj_state;

typedef struct wl_fd_queue {
    int data[WL_FDQ_CAP];
    uint32_t head;
    uint32_t tail;
} wl_fd_queue;

typedef struct wl_client_state {
    int fd;
    uint32_t pointer_id;
    uint32_t keyboard_id;
    uint32_t seat_id;
    uint32_t output_id;
    uint32_t xdg_wm_base_id;
    uint32_t focused_surface;
    uint32_t serial;
} wl_client_state;

static uint32_t* wl_fb = 0;
static uint32_t wl_fb_w = 1280;
static uint32_t wl_fb_h = 720;
static wl_obj_state g_objs[WL_OBJECT_MAX];
static wl_client_state g_client = {0};

static uint32_t g_pointer_x = 64;
static uint32_t g_pointer_y = 64;
static uint8_t g_pointer_buttons = 0;

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static uint32_t now_ms(void) {
    struct linux_timespec_local ts;
    long rc = (long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0);
    if (rc != 0) return 0;
    return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
}

static uint32_t rd_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) {
    wr_u32(out + at, v);
    return at + 4;
}

static uint32_t append_i32(uint8_t* out, uint32_t at, int32_t v) {
    wr_u32(out + at, (uint32_t)v);
    return at + 4;
}

static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) {
    uint32_t len = 0;
    while (s[len]) ++len;
    at = append_u32(out, at, len + 1);
    memcpy(out + at, s, len);
    out[at + len] = 0;
    at += len + 1;
    while (at & 3U) out[at++] = 0;
    return at;
}

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET;
        ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control;
        msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) {
    struct linux_iovec iov = {data, cap};
    uint8_t control[64] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    *recv_fd = -1;
    int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
    if (rc <= 0) return rc;
    if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS) {
            *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr));
        }
    }
    return rc;
}

static void fdq_init(wl_fd_queue* q) {
    q->head = 0;
    q->tail = 0;
    for (uint32_t i = 0; i < WL_FDQ_CAP; ++i) q->data[i] = -1;
}

static int fdq_push(wl_fd_queue* q, int fd) {
    uint32_t next = (q->tail + 1U) % WL_FDQ_CAP;
    if (next == q->head) return -1;
    q->data[q->tail] = fd;
    q->tail = next;
    return 0;
}

static int fdq_pop(wl_fd_queue* q) {
    if (q->head == q->tail) return -1;
    int fd = q->data[q->head];
    q->head = (q->head + 1U) % WL_FDQ_CAP;
    return fd;
}

static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size) {
    wr_u32(pkt + 0, obj_id);
    wr_u32(pkt + 4, ((uint32_t)size << 16) | opcode);
}

static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) {
    uint8_t pkt[256] = {0};
    uint32_t at = 0;
    at = append_u32(pkt, at, reg_id);
    uint32_t hdr_at = at;
    at = append_u32(pkt, at, 0);
    at = append_u32(pkt, at, name);
    at = append_string(pkt, at, iface);
    at = append_u32(pkt, at, version);
    wr_u32(pkt + hdr_at, (at << 16) | 0);
    (void)send_packet(fd, pkt, at, -1);
}

static void send_shm_format(int fd, uint32_t shm_id, uint32_t fmt) {
    uint8_t pkt[16] = {0};
    send_event_header(pkt, shm_id, 0, 12);
    wr_u32(pkt + 8, fmt);
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_output_geometry(int fd, uint32_t output_id) {
    uint8_t pkt[128] = {0};
    uint32_t at = 0;
    at = append_u32(pkt, at, output_id);
    uint32_t hdr_at = at;
    at = append_u32(pkt, at, 0);
    at = append_i32(pkt, at, 0);
    at = append_i32(pkt, at, 0);
    at = append_i32(pkt, at, 300);
    at = append_i32(pkt, at, 170);
    at = append_u32(pkt, at, 1);
    at = append_string(pkt, at, "AMS");
    at = append_string(pkt, at, "Virtual-0");
    at = append_i32(pkt, at, 0);
    wr_u32(pkt + hdr_at, (at << 16) | 0);
    (void)send_packet(fd, pkt, at, -1);
}

static void send_output_mode(int fd, uint32_t output_id) {
    uint8_t pkt[24] = {0};
    send_event_header(pkt, output_id, 1, 20);
    wr_u32(pkt + 8, 3);
    wr_u32(pkt + 12, wl_fb_w);
    wr_u32(pkt + 16, wl_fb_h);
    wr_u32(pkt + 20, 60000);
    (void)send_packet(fd, pkt, 24, -1);
}

static void send_output_done(int fd, uint32_t output_id) {
    uint8_t pkt[8] = {0};
    send_event_header(pkt, output_id, 2, 8);
    (void)send_packet(fd, pkt, 8, -1);
}

static void send_output_scale(int fd, uint32_t output_id, int32_t scale) {
    uint8_t pkt[12] = {0};
    send_event_header(pkt, output_id, 3, 12);
    wr_u32(pkt + 8, (uint32_t)scale);
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_seat_capabilities(int fd, uint32_t seat_id) {
    uint8_t pkt[12] = {0};
    send_event_header(pkt, seat_id, 0, 12);
    wr_u32(pkt + 8, WL_SEAT_CAP_POINTER | WL_SEAT_CAP_KEYBOARD);
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_seat_name(int fd, uint32_t seat_id, const char* name) {
    uint8_t pkt[96] = {0};
    uint32_t at = 0;
    at = append_u32(pkt, at, seat_id);
    uint32_t hdr_at = at;
    at = append_u32(pkt, at, 0);
    at = append_string(pkt, at, name);
    wr_u32(pkt + hdr_at, (at << 16) | 1);
    (void)send_packet(fd, pkt, at, -1);
}

static void send_callback_done(int fd, uint32_t cb_id, uint32_t ms) {
    uint8_t pkt[12] = {0};
    send_event_header(pkt, cb_id, 0, 12);
    wr_u32(pkt + 8, ms);
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_wm_ping(int fd, uint32_t wm_id, uint32_t serial) {
    uint8_t pkt[12] = {0};
    send_event_header(pkt, wm_id, 0, 12);
    wr_u32(pkt + 8, serial);
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_xdg_surface_configure(int fd, uint32_t xdg_surface_id, uint32_t serial) {
    uint8_t pkt[12] = {0};
    send_event_header(pkt, xdg_surface_id, 0, 12);
    wr_u32(pkt + 8, serial);
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_xdg_toplevel_configure(int fd, uint32_t toplevel_id, int32_t w, int32_t h) {
    uint8_t pkt[24] = {0};
    send_event_header(pkt, toplevel_id, 0, 20);
    wr_u32(pkt + 8, (uint32_t)w);
    wr_u32(pkt + 12, (uint32_t)h);
    wr_u32(pkt + 16, 0);
    (void)send_packet(fd, pkt, 20, -1);
}

static void send_xdg_toplevel_close(int fd, uint32_t toplevel_id) {
    uint8_t pkt[8] = {0};
    send_event_header(pkt, toplevel_id, 1, 8);
    (void)send_packet(fd, pkt, 8, -1);
}

static void send_pointer_enter(int fd, uint32_t pointer_id, uint32_t serial, uint32_t surface_id, uint32_t x, uint32_t y) {
    uint8_t pkt[24] = {0};
    send_event_header(pkt, pointer_id, 0, 24);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, surface_id);
    wr_u32(pkt + 16, x << 8);
    wr_u32(pkt + 20, y << 8);
    (void)send_packet(fd, pkt, 24, -1);
}

static void send_pointer_motion(int fd, uint32_t pointer_id, uint32_t time_ms, uint32_t x, uint32_t y) {
    uint8_t pkt[20] = {0};
    send_event_header(pkt, pointer_id, 2, 20);
    wr_u32(pkt + 8, time_ms);
    wr_u32(pkt + 12, x << 8);
    wr_u32(pkt + 16, y << 8);
    (void)send_packet(fd, pkt, 20, -1);
}

static void send_pointer_button(int fd, uint32_t pointer_id, uint32_t serial, uint32_t time_ms, uint32_t button, uint32_t state) {
    uint8_t pkt[24] = {0};
    send_event_header(pkt, pointer_id, 3, 24);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, time_ms);
    wr_u32(pkt + 16, button);
    wr_u32(pkt + 20, state);
    (void)send_packet(fd, pkt, 24, -1);
}

static void send_pointer_frame(int fd, uint32_t pointer_id) {
    uint8_t pkt[8] = {0};
    send_event_header(pkt, pointer_id, 5, 8);
    (void)send_packet(fd, pkt, 8, -1);
}

static void send_keyboard_enter(int fd, uint32_t keyboard_id, uint32_t serial, uint32_t surface_id) {
    uint8_t pkt[20] = {0};
    send_event_header(pkt, keyboard_id, 1, 20);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, surface_id);
    wr_u32(pkt + 16, 0);
    (void)send_packet(fd, pkt, 20, -1);
}

static void send_keyboard_key(int fd, uint32_t keyboard_id, uint32_t serial, uint32_t time_ms, uint32_t key, uint32_t state) {
    uint8_t pkt[24] = {0};
    send_event_header(pkt, keyboard_id, 3, 24);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, time_ms);
    wr_u32(pkt + 16, key);
    wr_u32(pkt + 20, state);
    (void)send_packet(fd, pkt, 24, -1);
}

static void send_keyboard_modifiers(int fd, uint32_t keyboard_id, uint32_t serial) {
    uint8_t pkt[28] = {0};
    send_event_header(pkt, keyboard_id, 4, 28);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, 0);
    wr_u32(pkt + 16, 0);
    wr_u32(pkt + 20, 0);
    wr_u32(pkt + 24, 0);
    (void)send_packet(fd, pkt, 28, -1);
}

static void send_buffer_release(int fd, uint32_t buffer_id) {
    uint8_t pkt[8] = {0};
    send_event_header(pkt, buffer_id, 0, 8);
    (void)send_packet(fd, pkt, 8, -1);
}

static void draw_shell_background(void) {
    if (!wl_fb) return;
    for (uint32_t y = 0; y < wl_fb_h; ++y) {
        uint32_t color = (y < 36) ? 0x1A1A1A : 0x10151D;
        for (uint32_t x = 0; x < wl_fb_w; ++x) {
            wl_fb[y * wl_fb_w + x] = color;
        }
    }
}

static void draw_pointer(void) {
    for (uint32_t y = 0; y < 12; ++y) {
        for (uint32_t x = 0; x < 10; ++x) {
            uint32_t px = g_pointer_x + x;
            uint32_t py = g_pointer_y + y;
            if (px >= wl_fb_w || py >= wl_fb_h) continue;
            wl_fb[py * wl_fb_w + px] = (x <= y) ? 0xFFFFFF : wl_fb[py * wl_fb_w + px];
        }
    }
}

static void present_attached_surface(uint32_t surf_id) {
    if (surf_id >= WL_OBJECT_MAX) return;
    wl_obj_state* surf = &g_objs[surf_id];
    if (surf->type != WL_SURFACE || surf->attached_buffer_id == 0) return;
    uint32_t bid = surf->attached_buffer_id;
    if (bid >= WL_OBJECT_MAX) return;
    wl_obj_state* buf = &g_objs[bid];
    if (buf->type != WL_BUFFER || buf->pool_id == 0 || buf->pool_id >= WL_OBJECT_MAX) return;
    wl_obj_state* pool = &g_objs[buf->pool_id];
    if (pool->type != WL_SHM_POOL || !pool->map || buf->format != WL_SHM_FORMAT_XRGB8888) return;

    draw_shell_background();
    uint32_t copy_w = (buf->width > 0 && (uint32_t)buf->width < wl_fb_w) ? (uint32_t)buf->width : wl_fb_w;
    uint32_t copy_h = (buf->height > 0 && (uint32_t)buf->height < wl_fb_h) ? (uint32_t)buf->height : wl_fb_h;
    for (uint32_t y = 0; y < copy_h; ++y) {
        uint32_t src_off = buf->offset + y * (uint32_t)buf->stride;
        if (src_off + copy_w * 4U > pool->size) break;
        memcpy(&wl_fb[y * wl_fb_w], pool->map + src_off, copy_w * 4U);
    }
    draw_pointer();
    (void)ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)wl_fb, wl_fb_w, wl_fb_h, 0, 0);

    if (surf->frame_callback_id && surf->frame_callback_id < WL_OBJECT_MAX) {
        send_callback_done((int)surf->client_fd, surf->frame_callback_id, now_ms());
        g_objs[surf->frame_callback_id].type = 0;
        surf->frame_callback_id = 0;
    }
    send_buffer_release((int)surf->client_fd, bid);
}

static void clear_objects(int fd) {
    memset(g_objs, 0, sizeof(g_objs));
    memset(&g_client, 0, sizeof(g_client));
    g_client.fd = fd;
    g_objs[1].type = WL_DISPLAY;
    g_objs[2].type = WL_COMPOSITOR;
    g_objs[3].type = WL_SHM;
}

static void handle_input_events(void) {
    if (g_client.fd <= 0) return;
    uint64_t mev = ams_syscall(SYS_AMS_GET_MOUSE_EVENT, 0, 0, 0, 0, 0);
    if (mev && g_client.pointer_id && g_client.focused_surface) {
        g_pointer_x = (uint32_t)(mev & 0xFFFFU);
        g_pointer_y = (uint32_t)((mev >> 16) & 0xFFFFU);
        uint8_t buttons = (uint8_t)((mev >> 32) & 0xFFU);
        uint8_t old_buttons = g_pointer_buttons;
        g_pointer_buttons = buttons;
        g_client.serial++;
        send_pointer_motion(g_client.fd, g_client.pointer_id, now_ms(), g_pointer_x, g_pointer_y);
        if (old_buttons != g_pointer_buttons) {
            uint32_t state = (g_pointer_buttons & 0x1) ? 1U : 0U;
            send_pointer_button(g_client.fd, g_client.pointer_id, g_client.serial, now_ms(), 0x110, state);
        }
        send_pointer_frame(g_client.fd, g_client.pointer_id);
    }

    uint64_t kev = ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
    if (kev && g_client.keyboard_id && g_client.focused_surface) {
        int32_t k = (int32_t)kev;
        uint32_t state = 1;
        if (k < 0) {
            state = 0;
            k = -k;
        }
        g_client.serial++;
        send_keyboard_key(g_client.fd, g_client.keyboard_id, g_client.serial, now_ms(), (uint32_t)k, state);
        send_keyboard_modifiers(g_client.fd, g_client.keyboard_id, g_client.serial);
    }
}

static void process_message(int fd, wl_fd_queue* fdq, uint32_t obj_id, uint16_t opcode, const uint8_t* payload, uint32_t payload_len) {
    if (obj_id >= WL_OBJECT_MAX) return;
    wl_obj_state* obj = &g_objs[obj_id];

    if (obj_id == 1 && obj->type == WL_DISPLAY) {
        if (opcode == 0 && payload_len >= 4) {
            uint32_t cb_id = rd_u32(payload);
            if (cb_id > 0 && cb_id < WL_OBJECT_MAX) {
                g_objs[cb_id].type = WL_CALLBACK;
                send_callback_done(fd, cb_id, now_ms());
            }
        } else if (opcode == 1 && payload_len >= 4) {
            uint32_t reg_id = rd_u32(payload);
            if (reg_id > 0 && reg_id < WL_OBJECT_MAX) {
                g_objs[reg_id].type = WL_REGISTRY;
                send_registry_global(fd, reg_id, 1, "wl_compositor", 4);
                send_registry_global(fd, reg_id, 2, "wl_shm", 1);
                send_registry_global(fd, reg_id, 3, "wl_output", 2);
                send_registry_global(fd, reg_id, 4, "wl_seat", 5);
                send_registry_global(fd, reg_id, 5, "xdg_wm_base", 1);
            }
        }
        return;
    }

    if (obj->type == WL_REGISTRY) {
        if (opcode == 0 && payload_len >= 16) {
            uint32_t name = rd_u32(payload + 0);
            uint32_t str_len = rd_u32(payload + 4);
            uint32_t str_padded = (str_len + 3U) & ~3U;
            if (payload_len < 4 + 4 + str_padded + 4 + 4) return;
            uint32_t new_id = rd_u32(payload + 12 + str_padded);
            if (new_id == 0 || new_id >= WL_OBJECT_MAX) return;
            if (name == 1) g_objs[new_id].type = WL_COMPOSITOR;
            if (name == 2) {
                g_objs[new_id].type = WL_SHM;
                send_shm_format(fd, new_id, WL_SHM_FORMAT_XRGB8888);
            }
            if (name == 3) {
                g_objs[new_id].type = WL_OUTPUT;
                g_client.output_id = new_id;
                send_output_geometry(fd, new_id);
                send_output_mode(fd, new_id);
                send_output_scale(fd, new_id, 1);
                send_output_done(fd, new_id);
            }
            if (name == 4) {
                g_objs[new_id].type = WL_SEAT;
                g_client.seat_id = new_id;
                send_seat_capabilities(fd, new_id);
                send_seat_name(fd, new_id, "seat0");
            }
            if (name == 5) {
                g_objs[new_id].type = XDG_WM_BASE;
                g_client.xdg_wm_base_id = new_id;
                g_client.serial++;
                send_wm_ping(fd, new_id, g_client.serial);
            }
        }
        return;
    }

    if (obj->type == WL_COMPOSITOR) {
        if (opcode == 0 && payload_len >= 4) {
            uint32_t new_id = rd_u32(payload + 0);
            if (new_id > 0 && new_id < WL_OBJECT_MAX) {
                g_objs[new_id].type = WL_SURFACE;
                g_objs[new_id].client_fd = (uint32_t)fd;
            }
        }
        return;
    }

    if (obj->type == WL_SHM) {
        if (opcode == 0 && payload_len >= 8) {
            uint32_t new_id = rd_u32(payload + 0);
            uint32_t size = rd_u32(payload + 4);
            int passed_fd = fdq_pop(fdq);
            if (new_id == 0 || new_id >= WL_OBJECT_MAX) return;
            if (passed_fd < 0 || size == 0) return;
            g_objs[new_id].type = WL_SHM_POOL;
            g_objs[new_id].fd = passed_fd;
            g_objs[new_id].size = size;
            g_objs[new_id].map = (uint8_t*)mmap(0, size, PROT_READ, MAP_SHARED, passed_fd, 0);
            if ((uint64_t)g_objs[new_id].map > (uint64_t)-4096LL) g_objs[new_id].map = 0;
        }
        return;
    }

    if (obj->type == WL_SHM_POOL) {
        if (opcode == 0 && payload_len >= 24) {
            uint32_t new_id = rd_u32(payload + 0);
            if (new_id == 0 || new_id >= WL_OBJECT_MAX) return;
            g_objs[new_id].type = WL_BUFFER;
            g_objs[new_id].pool_id = obj_id;
            g_objs[new_id].offset = rd_u32(payload + 4);
            g_objs[new_id].width = (int32_t)rd_u32(payload + 8);
            g_objs[new_id].height = (int32_t)rd_u32(payload + 12);
            g_objs[new_id].stride = (int32_t)rd_u32(payload + 16);
            g_objs[new_id].format = rd_u32(payload + 20);
            g_objs[new_id].client_fd = (uint32_t)fd;
        } else if (opcode == 1) {
            obj->type = 0;
        }
        return;
    }

    if (obj->type == WL_SURFACE) {
        if (opcode == 1 && payload_len >= 12) {
            obj->attached_buffer_id = rd_u32(payload + 0);
        } else if (opcode == 3 && payload_len >= 4) {
            uint32_t cb = rd_u32(payload + 0);
            obj->frame_callback_id = cb;
            if (cb > 0 && cb < WL_OBJECT_MAX) g_objs[cb].type = WL_CALLBACK;
        } else if (opcode == 6) {
            g_client.focused_surface = obj_id;
            present_attached_surface(obj_id);
            if (g_client.pointer_id) send_pointer_enter(fd, g_client.pointer_id, ++g_client.serial, obj_id, g_pointer_x, g_pointer_y);
            if (g_client.keyboard_id) send_keyboard_enter(fd, g_client.keyboard_id, ++g_client.serial, obj_id);
        } else if (opcode == 0) {
            obj->type = 0;
        }
        return;
    }

    if (obj->type == WL_SEAT) {
        if (opcode == 0 && payload_len >= 4) {
            uint32_t nid = rd_u32(payload + 0);
            if (nid && nid < WL_OBJECT_MAX) {
                g_objs[nid].type = WL_POINTER;
                g_client.pointer_id = nid;
            }
        } else if (opcode == 1 && payload_len >= 4) {
            uint32_t nid = rd_u32(payload + 0);
            if (nid && nid < WL_OBJECT_MAX) {
                g_objs[nid].type = WL_KEYBOARD;
                g_client.keyboard_id = nid;
            }
        }
        return;
    }

    if (obj->type == XDG_WM_BASE) {
        if (opcode == 1 && payload_len >= 8) {
            uint32_t xdg_surface_id = rd_u32(payload + 0);
            uint32_t surf_id = rd_u32(payload + 4);
            if (xdg_surface_id && xdg_surface_id < WL_OBJECT_MAX && surf_id && surf_id < WL_OBJECT_MAX) {
                g_objs[xdg_surface_id].type = XDG_SURFACE;
                g_objs[xdg_surface_id].role_id = surf_id;
                g_objs[xdg_surface_id].client_fd = (uint32_t)fd;
                g_objs[surf_id].role_id = xdg_surface_id;
                send_xdg_surface_configure(fd, xdg_surface_id, ++g_client.serial);
            }
        }
        return;
    }

    if (obj->type == XDG_SURFACE) {
        if (opcode == 1 && payload_len >= 4) {
            uint32_t toplevel_id = rd_u32(payload + 0);
            if (toplevel_id && toplevel_id < WL_OBJECT_MAX) {
                g_objs[toplevel_id].type = XDG_TOPLEVEL;
                g_objs[toplevel_id].role_id = obj_id;
                g_objs[toplevel_id].client_fd = (uint32_t)fd;
                send_xdg_toplevel_configure(fd, toplevel_id, (int32_t)wl_fb_w, (int32_t)wl_fb_h);
                send_xdg_surface_configure(fd, obj_id, ++g_client.serial);
            }
        } else if (opcode == 4 && payload_len >= 4) {
            (void)rd_u32(payload + 0);
        } else if (opcode == 0) {
            obj->type = 0;
        }
        return;
    }

    if (obj->type == XDG_TOPLEVEL) {
        if (opcode == 0 && payload_len >= 4) {
            uint32_t slen = rd_u32(payload);
            (void)slen;
        } else if (opcode == 1 && payload_len >= 4) {
            uint32_t slen = rd_u32(payload);
            (void)slen;
        } else if (opcode == 0xFFFF) {
            send_xdg_toplevel_close(fd, obj_id);
        }
        return;
    }

    if (obj->type == WL_BUFFER && opcode == 0) {
        obj->type = 0;
    }
}

static int bootstrap_local_shell_client(void) {
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    {
        const char* p = "/run/user/0/wayland-0";
        for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i];
    }

    int fd = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (fd < 0) return -1;
    if ((int)ams_syscall(SYS_CONNECT, (uint64_t)fd, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) return -2;

    const uint32_t registry_id = 40;
    const uint32_t compositor_id = 41;
    const uint32_t shm_id = 42;
    const uint32_t surface_id = 43;
    const uint32_t pool_id = 44;
    const uint32_t buffer_id = 45;
    const uint32_t xdg_wm_base_id = 46;
    const uint32_t xdg_surface_id = 47;
    const uint32_t xdg_toplevel_id = 48;
    uint8_t msg[512] = {0};
    uint32_t at = 0;

    at = append_u32(msg, at, 1);
    at = append_u32(msg, at, (12U << 16) | 1U);
    at = append_u32(msg, at, registry_id);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, registry_id);
    uint32_t h = at;
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, 1);
    at = append_string(msg, at, "wl_compositor");
    at = append_u32(msg, at, 4);
    at = append_u32(msg, at, compositor_id);
    wr_u32(msg + h, (at << 16) | 0U);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, registry_id);
    h = at;
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, 2);
    at = append_string(msg, at, "wl_shm");
    at = append_u32(msg, at, 1);
    at = append_u32(msg, at, shm_id);
    wr_u32(msg + h, (at << 16) | 0U);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, registry_id);
    h = at;
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, 5);
    at = append_string(msg, at, "xdg_wm_base");
    at = append_u32(msg, at, 1);
    at = append_u32(msg, at, xdg_wm_base_id);
    wr_u32(msg + h, (at << 16) | 0U);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, compositor_id);
    at = append_u32(msg, at, (12U << 16) | 0U);
    at = append_u32(msg, at, surface_id);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, xdg_wm_base_id);
    at = append_u32(msg, at, (16U << 16) | 1U);
    at = append_u32(msg, at, xdg_surface_id);
    at = append_u32(msg, at, surface_id);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, xdg_surface_id);
    at = append_u32(msg, at, (12U << 16) | 1U);
    at = append_u32(msg, at, xdg_toplevel_id);
    (void)send_packet(fd, msg, at, -1);

    const int width = 960;
    const int height = 540;
    const int stride = width * 4;
    const int size = stride * height;
    int shmfd = (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)"wl-shell", 0, 0, 0, 0);
    if (shmfd < 0) return -3;
    if ((int)ams_syscall(SYS_FTRUNCATE, (uint64_t)shmfd, (uint64_t)size, 0, 0, 0) < 0) return -4;
    uint32_t* pix = (uint32_t*)mmap(0, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if ((uint64_t)pix > (uint64_t)-4096LL) return -5;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t c = (y < 40) ? 0x243248 : 0x1A2130;
            pix[y * width + x] = c;
        }
    }

    at = 0;
    at = append_u32(msg, at, shm_id);
    at = append_u32(msg, at, (16U << 16) | 0U);
    at = append_u32(msg, at, pool_id);
    at = append_u32(msg, at, (uint32_t)size);
    (void)send_packet(fd, msg, at, shmfd);

    at = 0;
    at = append_u32(msg, at, pool_id);
    at = append_u32(msg, at, (32U << 16) | 0U);
    at = append_u32(msg, at, buffer_id);
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, (uint32_t)width);
    at = append_u32(msg, at, (uint32_t)height);
    at = append_u32(msg, at, (uint32_t)stride);
    at = append_u32(msg, at, 0);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, surface_id);
    at = append_u32(msg, at, (20U << 16) | 1U);
    at = append_u32(msg, at, buffer_id);
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, 0);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, surface_id);
    at = append_u32(msg, at, (8U << 16) | 6U);
    (void)send_packet(fd, msg, at, -1);
    return fd;
}

static void handle_client_session(int cli) {
    uint8_t rx[WL_RX_CAP];
    uint32_t rx_len = 0;
    wl_fd_queue fdq;
    fdq_init(&fdq);
    clear_objects(cli);
    puts1("wl-compositor: client connected");

    uint32_t last_idle = now_ms();
    while (1) {
        int passed_fd = -1;
        int n = recv_packet(cli, rx + rx_len, WL_RX_CAP - rx_len, &passed_fd);
        if (n == 0) break;
        if (n < 0) {
            handle_input_events();
            if ((uint32_t)(now_ms() - last_idle) > 16) {
                draw_shell_background();
                draw_pointer();
                (void)ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)wl_fb, wl_fb_w, wl_fb_h, 0, 0);
                last_idle = now_ms();
            }
            continue;
        }
        if (passed_fd >= 0) (void)fdq_push(&fdq, passed_fd);
        rx_len += (uint32_t)n;
        uint32_t at = 0;
        while (rx_len - at >= 8) {
            uint32_t obj_id = rd_u32(rx + at);
            uint32_t hdr = rd_u32(rx + at + 4);
            uint16_t opcode = (uint16_t)(hdr & 0xFFFFU);
            uint16_t size = (uint16_t)(hdr >> 16);
            if (size < 8 || at + size > rx_len) break;
            process_message(cli, &fdq, obj_id, opcode, rx + at + 8, (uint32_t)size - 8);
            at += size;
        }
        if (at > 0) {
            memmove(rx, rx + at, rx_len - at);
            rx_len -= at;
        }
        handle_input_events();
    }
    (void)ams_syscall(SYS_CLOSE, cli, 0, 0, 0, 0);
    puts1("wl-compositor: client disconnected");
}

int main(void) {
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    {
        const char* p = "/run/user/0/wayland-0";
        for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i];
    }

    int srv = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (srv < 0) {
        puts1("wl-compositor: socket failed");
        return 1;
    }
    if ((int)ams_syscall(SYS_BIND, srv, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        puts1("wl-compositor: bind failed");
        return 2;
    }
    (void)ams_syscall(SYS_LISTEN, srv, 8, 0, 0, 0);
    puts1("wl-compositor: listening on wayland-0");

    if ((int)ams_syscall(SYS_AMS_GET_FB_INFO, (uint64_t)&wl_fb_w, (uint64_t)&wl_fb_h, 0, 0, 0) != 0 || wl_fb_w == 0 || wl_fb_h == 0) {
        wl_fb_w = 1280;
        wl_fb_h = 720;
    }
    wl_fb = (uint32_t*)malloc((size_t)wl_fb_w * (size_t)wl_fb_h * sizeof(uint32_t));
    if (!wl_fb) {
        puts1("wl-compositor: fb buffer alloc failed");
        return 3;
    }
    draw_shell_background();
    draw_pointer();
    (void)ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)wl_fb, wl_fb_w, wl_fb_h, 0, 0);

    puts1("wl-compositor: protocol core+xdg+seat ready");
    if (bootstrap_local_shell_client() >= 0) {
        puts1("wl-compositor: local shell bootstrap queued");
    } else {
        puts1("wl-compositor: local shell bootstrap failed");
    }

    while (1) {
        int cli = (int)ams_syscall(SYS_ACCEPT, srv, 0, 0, 0, 0);
        if (cli < 0) {
            handle_input_events();
            continue;
        }
        handle_client_session(cli);
    }
}
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43
#define SYS_CONNECT 42
#define SYS_SENDMSG 46
#define SYS_RECVMSG 47
#define SYS_CLOSE 3
#define SYS_MEMFD_CREATE 319
#define SYS_FTRUNCATE 77
#define SYS_CLOCK_GETTIME 228
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01

#define WL_OBJECT_MAX 256
#define WL_RX_CAP 8192
#define WL_FDQ_CAP 32

#define WL_DISPLAY 1
#define WL_REGISTRY 2
#define WL_COMPOSITOR 3
#define WL_SHM 4
#define WL_SURFACE 5
#define WL_SHM_POOL 6
#define WL_BUFFER 7

#define WL_SHM_FORMAT_XRGB8888 0

struct linux_sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

struct linux_iovec {
    void* iov_base;
    uint64_t iov_len;
};

struct linux_msghdr {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    struct linux_iovec* msg_iov;
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

struct linux_timespec_local {
    int64_t tv_sec;
    int64_t tv_nsec;
};

typedef struct wl_obj_state {
    uint32_t type;
    int fd;
    uint8_t* map;
    uint32_t size;
    uint32_t pool_id;
    uint32_t offset;
    int32_t width;
    int32_t height;
    int32_t stride;
    uint32_t format;
    uint32_t attached_buffer_id;
} wl_obj_state;

static uint32_t* wl_fb = 0;
static uint32_t wl_fb_w = 1280;
static uint32_t wl_fb_h = 720;
static int wl_blit_failed_once = 0;
static wl_obj_state g_objs[WL_OBJECT_MAX];

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static uint32_t now_ms(void) {
    struct linux_timespec_local ts;
    long rc = (long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0);
    if (rc != 0) return 0;
    return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
}

static uint32_t rd_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) {
    wr_u32(out + at, v);
    return at + 4;
}

static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) {
    uint32_t len = 0;
    while (s[len]) ++len;
    at = append_u32(out, at, len + 1);
    memcpy(out + at, s, len);
    out[at + len] = 0;
    at += len + 1;
    while (at & 3U) out[at++] = 0;
    return at;
}

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET;
        ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control;
        msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) {
    struct linux_iovec iov = {data, cap};
    uint8_t control[64] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    *recv_fd = -1;
    int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
    if (rc <= 0) return rc;
    if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS) {
            *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr));
        }
    }
    return rc;
}

static int g_bootstrap_client_fd = -1;

typedef struct wl_fd_queue {
    int data[WL_FDQ_CAP];
    uint32_t head;
    uint32_t tail;
} wl_fd_queue;

static void fdq_init(wl_fd_queue* q) {
    q->head = 0;
    q->tail = 0;
    for (uint32_t i = 0; i < WL_FDQ_CAP; ++i) q->data[i] = -1;
}

static int fdq_push(wl_fd_queue* q, int fd) {
    uint32_t next = (q->tail + 1U) % WL_FDQ_CAP;
    if (next == q->head) return -1; // full
    q->data[q->tail] = fd;
    q->tail = next;
    return 0;
}

static int fdq_pop(wl_fd_queue* q) {
    if (q->head == q->tail) return -1; // empty
    int fd = q->data[q->head];
    q->head = (q->head + 1U) % WL_FDQ_CAP;
    return fd;
}

static int bootstrap_local_shm_client(void) {
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    {
        const char* p = "/run/user/0/wayland-0";
        for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i];
    }

    int fd = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (fd < 0) return -1;
    if ((int)ams_syscall(SYS_CONNECT, (uint64_t)fd, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        (void)ams_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
        return -2;
    }

    // Minimalny bootstrap klienta z ustalonymi nazwami globals 1/2.
    const uint32_t registry_id = 10;
    const uint32_t compositor_id = 11;
    const uint32_t shm_id = 12;
    const uint32_t surface_id = 13;
    const uint32_t pool_id = 14;
    const uint32_t buffer_id = 15;
    uint8_t msg[512] = {0};
    uint32_t at = 0;

    at = append_u32(msg, at, 1);
    at = append_u32(msg, at, (12U << 16) | 1U); // wl_display.get_registry
    at = append_u32(msg, at, registry_id);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, registry_id);
    {
        uint32_t hdr_at = at;
        at = append_u32(msg, at, 0); // wl_registry.bind
        at = append_u32(msg, at, 1); // wl_compositor global name
        at = append_string(msg, at, "wl_compositor");
        at = append_u32(msg, at, 4);
        at = append_u32(msg, at, compositor_id);
        wr_u32(msg + hdr_at, (uint32_t)(at << 16) | 0U);
    }
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, registry_id);
    {
        uint32_t hdr_at = at;
        at = append_u32(msg, at, 0); // wl_registry.bind
        at = append_u32(msg, at, 2); // wl_shm global name
        at = append_string(msg, at, "wl_shm");
        at = append_u32(msg, at, 1);
        at = append_u32(msg, at, shm_id);
        wr_u32(msg + hdr_at, (uint32_t)(at << 16) | 0U);
    }
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, compositor_id);
    at = append_u32(msg, at, (12U << 16) | 0U); // create_surface
    at = append_u32(msg, at, surface_id);
    (void)send_packet(fd, msg, at, -1);

    const int width = 640;
    const int height = 360;
    const int stride = width * 4;
    const int size = stride * height;
    int shmfd = (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)"wl-bootstrap", 0, 0, 0, 0);
    if (shmfd < 0) return -3;
    if ((int)ams_syscall(SYS_FTRUNCATE, (uint64_t)shmfd, (uint64_t)size, 0, 0, 0) < 0) return -4;
    uint32_t* pix = (uint32_t*)mmap(0, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if ((uint64_t)pix > (uint64_t)-4096LL) return -5;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t r = (uint32_t)((x * 255) / width);
            uint32_t g = (uint32_t)((y * 255) / height);
            uint32_t b = (uint32_t)((x ^ y) & 0xFF);
            pix[y * width + x] = (r << 16) | (g << 8) | b;
        }
    }

    at = 0;
    at = append_u32(msg, at, shm_id);
    at = append_u32(msg, at, (16U << 16) | 0U); // create_pool
    at = append_u32(msg, at, pool_id);
    at = append_u32(msg, at, (uint32_t)size);
    (void)send_packet(fd, msg, at, shmfd);

    at = 0;
    at = append_u32(msg, at, pool_id);
    at = append_u32(msg, at, (32U << 16) | 0U); // create_buffer
    at = append_u32(msg, at, buffer_id);
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, (uint32_t)width);
    at = append_u32(msg, at, (uint32_t)height);
    at = append_u32(msg, at, (uint32_t)stride);
    at = append_u32(msg, at, 0); // XRGB8888
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, surface_id);
    at = append_u32(msg, at, (20U << 16) | 1U); // attach
    at = append_u32(msg, at, buffer_id);
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, 0);
    (void)send_packet(fd, msg, at, -1);

    at = 0;
    at = append_u32(msg, at, surface_id);
    at = append_u32(msg, at, (8U << 16) | 6U); // commit
    (void)send_packet(fd, msg, at, -1);

    g_bootstrap_client_fd = fd;
    return fd;
}

static void send_registry_global(int fd, uint32_t reg_id, uint32_t name, const char* iface, uint32_t version) {
    uint8_t pkt[256] = {0};
    uint32_t at = 0;
    at = append_u32(pkt, at, reg_id);
    uint32_t hdr_at = at;
    at = append_u32(pkt, at, 0);
    at = append_u32(pkt, at, name);
    at = append_string(pkt, at, iface);
    at = append_u32(pkt, at, version);
    wr_u32(pkt + hdr_at, (at << 16) | 0); // wl_registry.global
    (void)send_packet(fd, pkt, at, -1);
}

static void send_shm_format(int fd, uint32_t shm_id, uint32_t fmt) {
    uint8_t pkt[16] = {0};
    wr_u32(pkt + 0, shm_id);
    wr_u32(pkt + 4, (12U << 16) | 0U); // wl_shm.format
    wr_u32(pkt + 8, fmt);
    (void)send_packet(fd, pkt, 12, -1);
}

static void draw_waiting_frame(uint32_t tick) {
    if (!wl_fb) return;
    for (uint32_t y = 0; y < wl_fb_h; ++y) {
        for (uint32_t x = 0; x < wl_fb_w; ++x) {
            uint32_t r = (uint32_t)((x + tick) & 0xFF);
            uint32_t g = (uint32_t)((y + (tick >> 1)) & 0xFF);
            uint32_t b = 0x40;
            wl_fb[y * wl_fb_w + x] = (r << 16) | (g << 8) | b;
        }
    }
    long rc = (long)ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)wl_fb, wl_fb_w, wl_fb_h, 0, 0);
    if (rc != 0 && !wl_blit_failed_once) {
        wl_blit_failed_once = 1;
        puts1("wl-compositor: fb blit failed");
    }
}

static void present_attached_surface(uint32_t surf_id) {
    if (surf_id >= WL_OBJECT_MAX) return;
    wl_obj_state* surf = &g_objs[surf_id];
    if (surf->type != WL_SURFACE || surf->attached_buffer_id == 0) return;
    uint32_t bid = surf->attached_buffer_id;
    if (bid >= WL_OBJECT_MAX) return;
    wl_obj_state* buf = &g_objs[bid];
    if (buf->type != WL_BUFFER) return;
    if (buf->pool_id == 0 || buf->pool_id >= WL_OBJECT_MAX) return;
    wl_obj_state* pool = &g_objs[buf->pool_id];
    if (pool->type != WL_SHM_POOL || !pool->map) return;
    if (buf->format != WL_SHM_FORMAT_XRGB8888) return;

    memset(wl_fb, 0, wl_fb_w * wl_fb_h * sizeof(uint32_t));

    uint32_t copy_w = (buf->width > 0 && (uint32_t)buf->width < wl_fb_w) ? (uint32_t)buf->width : wl_fb_w;
    uint32_t copy_h = (buf->height > 0 && (uint32_t)buf->height < wl_fb_h) ? (uint32_t)buf->height : wl_fb_h;
    for (uint32_t y = 0; y < copy_h; ++y) {
        uint32_t src_off = buf->offset + y * (uint32_t)buf->stride;
        if (src_off + copy_w * 4U > pool->size) break;
        memcpy(&wl_fb[y * wl_fb_w], pool->map + src_off, copy_w * 4U);
    }
    (void)ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)wl_fb, wl_fb_w, wl_fb_h, 0, 0);
}

static void clear_objects(void) {
    memset(g_objs, 0, sizeof(g_objs));
    g_objs[1].type = WL_DISPLAY;
    g_objs[2].type = WL_COMPOSITOR;
    g_objs[3].type = WL_SHM;
}

static void process_message(int fd, wl_fd_queue* fdq, uint32_t obj_id, uint16_t opcode, const uint8_t* payload, uint32_t payload_len) {
    if (obj_id >= WL_OBJECT_MAX) return;
    wl_obj_state* obj = &g_objs[obj_id];
    if (obj_id == 1 && obj->type == WL_DISPLAY) {
        if (opcode == 1 && payload_len >= 4) { // get_registry(new_id)
            uint32_t reg_id = rd_u32(payload);
            if (reg_id > 0 && reg_id < WL_OBJECT_MAX) {
                g_objs[reg_id].type = WL_REGISTRY;
                send_registry_global(fd, reg_id, 1, "wl_compositor", 4);
                send_registry_global(fd, reg_id, 2, "wl_shm", 1);
            }
        }
        return;
    }

    if (obj->type == WL_REGISTRY) {
        if (opcode == 0 && payload_len >= 16) { // bind
            uint32_t name = rd_u32(payload + 0);
            uint32_t str_len = rd_u32(payload + 4);
            uint32_t str_padded = (str_len + 3U) & ~3U;
            if (payload_len < 4 + 4 + str_padded + 4 + 4) return;
            uint32_t version = rd_u32(payload + 8 + str_padded);
            uint32_t new_id = rd_u32(payload + 12 + str_padded);
            if (new_id == 0 || new_id >= WL_OBJECT_MAX) return;
            if (name == 1) g_objs[new_id].type = WL_COMPOSITOR;
            if (name == 2) {
                g_objs[new_id].type = WL_SHM;
                send_shm_format(fd, new_id, WL_SHM_FORMAT_XRGB8888);
                (void)version;
            }
        }
        return;
    }

    if (obj->type == WL_COMPOSITOR) {
        if (opcode == 0 && payload_len >= 4) { // create_surface
            uint32_t new_id = rd_u32(payload + 0);
            if (new_id > 0 && new_id < WL_OBJECT_MAX) g_objs[new_id].type = WL_SURFACE;
        }
        return;
    }

    if (obj->type == WL_SHM) {
        if (opcode == 0 && payload_len >= 8) { // create_pool
            uint32_t new_id = rd_u32(payload + 0);
            uint32_t size = rd_u32(payload + 4);
            int passed_fd = fdq_pop(fdq);
            if (new_id == 0 || new_id >= WL_OBJECT_MAX) return;
            if (passed_fd < 0 || size == 0) return;
            g_objs[new_id].type = WL_SHM_POOL;
            g_objs[new_id].fd = passed_fd;
            g_objs[new_id].size = size;
            g_objs[new_id].map = (uint8_t*)mmap(0, size, PROT_READ, MAP_SHARED, passed_fd, 0);
            if ((uint64_t)g_objs[new_id].map > (uint64_t)-4096LL) g_objs[new_id].map = 0;
        }
        return;
    }

    if (obj->type == WL_SHM_POOL) {
        if (opcode == 0 && payload_len >= 24) { // create_buffer
            uint32_t new_id = rd_u32(payload + 0);
            if (new_id == 0 || new_id >= WL_OBJECT_MAX) return;
            g_objs[new_id].type = WL_BUFFER;
            g_objs[new_id].pool_id = obj_id;
            g_objs[new_id].offset = rd_u32(payload + 4);
            g_objs[new_id].width = (int32_t)rd_u32(payload + 8);
            g_objs[new_id].height = (int32_t)rd_u32(payload + 12);
            g_objs[new_id].stride = (int32_t)rd_u32(payload + 16);
            g_objs[new_id].format = rd_u32(payload + 20);
        } else if (opcode == 1) { // destroy
            obj->type = 0;
        }
        return;
    }

    if (obj->type == WL_SURFACE) {
        if (opcode == 1 && payload_len >= 12) { // attach
            uint32_t buffer_id = rd_u32(payload + 0);
            obj->attached_buffer_id = buffer_id;
        } else if (opcode == 6) { // commit
            present_attached_surface(obj_id);
        } else if (opcode == 0) { // destroy
            obj->type = 0;
        }
        return;
    }

    if (obj->type == WL_BUFFER && opcode == 0) { // destroy
        obj->type = 0;
    }
}

static void handle_client_session(int cli) {
    uint8_t rx[WL_RX_CAP];
    uint32_t rx_len = 0;
    wl_fd_queue fdq;
    fdq_init(&fdq);
    clear_objects();
    puts1("wl-compositor: client connected");
    while (1) {
        int passed_fd = -1;
        int n = recv_packet(cli, rx + rx_len, WL_RX_CAP - rx_len, &passed_fd);
        if (n == 0) break;
        if (n < 0) continue;
        if (passed_fd >= 0) (void)fdq_push(&fdq, passed_fd);
        rx_len += (uint32_t)n;
        uint32_t at = 0;
        while (rx_len - at >= 8) {
            uint32_t obj_id = rd_u32(rx + at);
            uint32_t hdr = rd_u32(rx + at + 4);
            uint16_t opcode = (uint16_t)(hdr & 0xFFFFU);
            uint16_t size = (uint16_t)(hdr >> 16);
            if (size < 8) {
                at = rx_len;
                break;
            }
            if (at + size > rx_len) break;
            process_message(cli, &fdq, obj_id, opcode, rx + at + 8, (uint32_t)size - 8);
            at += size;
        }
        if (at > 0) {
            memmove(rx, rx + at, rx_len - at);
            rx_len -= at;
        }
    }
    (void)ams_syscall(SYS_CLOSE, cli, 0, 0, 0, 0);
    puts1("wl-compositor: client disconnected");
}

int main(void) {
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    {
        const char* p = "/run/user/0/wayland-0";
        for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i];
    }

    int srv = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (srv < 0) {
        puts1("wl-compositor: socket failed");
        return 1;
    }
    if ((int)ams_syscall(SYS_BIND, srv, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        puts1("wl-compositor: bind failed");
        return 2;
    }
    (void)ams_syscall(SYS_LISTEN, srv, 8, 0, 0, 0);
    puts1("wl-compositor: listening on wayland-0");

    if ((int)ams_syscall(SYS_AMS_GET_FB_INFO, (uint64_t)&wl_fb_w, (uint64_t)&wl_fb_h, 0, 0, 0) != 0 || wl_fb_w == 0 || wl_fb_h == 0) {
        wl_fb_w = 1280;
        wl_fb_h = 720;
    }
    wl_fb = (uint32_t*)malloc((size_t)wl_fb_w * (size_t)wl_fb_h * sizeof(uint32_t));
    if (!wl_fb) {
        puts1("wl-compositor: fb buffer alloc failed");
        return 3;
    }
    draw_waiting_frame(0);
    puts1("wl-compositor: protocol core ready");
    if (bootstrap_local_shm_client() >= 0) {
        puts1("wl-compositor: local shm bootstrap queued");
    } else {
        puts1("wl-compositor: local shm bootstrap failed");
    }

    uint32_t tick = 1;
    uint32_t last_draw = now_ms();
    uint32_t last_wait_log = now_ms();
    while (1) {
        uint32_t tnow = now_ms();
        if ((uint32_t)(tnow - last_draw) >= 120) {
            draw_waiting_frame(tick++);
            last_draw = tnow;
        }
        int cli = (int)ams_syscall(SYS_ACCEPT, srv, 0, 0, 0, 0);
        if (cli < 0) {
            if ((uint32_t)(tnow - last_wait_log) >= 2000) {
                puts1("wl-compositor: waiting for client...");
                last_wait_log = tnow;
            }
            continue;
        }
        handle_client_session(cli);
    }
}
