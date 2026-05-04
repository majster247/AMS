# Wayland desktop graphics stack

[← powrót do indeksu](./index.md)

AMS-OS dostarcza pełen, modularny Wayland desktop stack. Dotychczasowy
ręcznie pisany kompozytor (~2300 linii ad-hoc obsługi protokołu)
został usunięty i zastąpiony stosem opartym o oryginalne, upstreamowe
komponenty.

## Komponenty

| Warstwa | Źródło | Lokalizacja |
| --- | --- | --- |
| libc      | mlibc                                                        | `external/wayland-stack/mlibc` |
| FFI       | libffi                                                       | `external/wayland-stack/libffi` |
| 2D render | pixman, cairo                                                | `external/wayland-stack/{pixman,cairo}` |
| Klawiatura | libxkbcommon                                                | `external/wayland-stack/libxkbcommon` |
| Wejście   | libinput                                                     | `external/wayland-stack/libinput` |
| Wayland   | libwayland + wayland-protocols                                | `external/wayland-stack/{wayland,wayland-protocols}` |
| GPU sw    | Mesa3D (EGL + GBM, swrast/llvmpipe)                            | `external/wayland-stack/mesa` |
| Compositor | wlroots                                                     | `external/wayland-stack/wlroots` |
| AMS glue  | libports + ams-compositor                                    | `src/libports/`, `src/apps/wayland/` |

Wersje są przypięte tagami w `tools/wayland_stage.sh`
(np. `wayland 1.23.0`, `mesa 24.2.4`, `wlroots 0.18.1`).

## Kernel-side: `/dev/dri/card0` (DRM/KMS/GEM/TTM)

Plik `src/drivers/drm/ams_drm.cpp` implementuje "software-first"
fasadę DRM zgodną z ABI libdrm. Userspace może:

* `open("/dev/dri/card0", O_RDWR)`,
* wywołać `DRM_IOCTL_MODE_CREATE_DUMB` aby zaalokować bufor GEM
  (placement TTM = `SYSTEM`),
* wywołać `DRM_IOCTL_MODE_MAP_DUMB` i `mmap()` na zwróconym
  offsecie aby uzyskać wskaźnik użytkowy,
* `DRM_IOCTL_MODE_ADDFB2` rejestruje framebuffer KMS,
* `DRM_IOCTL_MODE_PAGE_FLIP` jest no-op w wariancie software (kompozytor
  rysuje przez pixman/cairo).

Karta jest jedna (eDP-1 wirtualna), z jednym CRTC, jednym enkoderem
i dwiema płaszczyznami.

## Kernel-side: shm_open, poll/epoll, AF_UNIX, libffi

* `shm_open()` w userspace (`libports/libports_shm.c`) sprowadza się
  do `memfd_create()` w jądrze. Deskryptor obsługuje `ftruncate`,
  `mmap(MAP_SHARED)` oraz przekazywanie przez `SCM_RIGHTS`.
* `poll`, `ppoll`, `epoll_create1`, `epoll_ctl`, `epoll_wait` mają
  pełne implementacje w `src/arch/x86_64/syscall.cpp`. libports
  publikuje glibc-zgodne aliasy.
* AF_UNIX z `sendmsg`/`recvmsg` + `SCM_RIGHTS` jest podstawą
  Wayland IPC; szkielet w jądrze obsługuje 100 socketów na system.
* libffi jest portowane bez modyfikacji upstream — `libports_ffi.c`
  dodaje tylko `getauxval(AT_PAGESZ)` i `sysconf(_SC_PAGESIZE)`.

## Mesa3D (EGL + GBM)

`tools/wayland_build.sh` konfiguruje meson z:

```
-Dgallium-drivers=swrast,llvmpipe
-Dplatforms=wayland
-Dgbm=enabled -Degl=enabled -Dosmesa=true
```

Render odbywa się software'owo (swrast lub llvmpipe gdy jest LLVM).
GBM komunikuje się z naszym `/dev/dri/card0` przez `libports_drm.c`.

## wlroots

wlroots jest budowany jako shared library i linkowany przez
`build/ams-compositor.elf`. Wykorzystywane backendy:

* **scene graph** — wlroots renderer pixman (software).
* **xdg-shell** — top-level shell protocol (wszystkie aplikacje desktop).
* **seat / input** — libinput dostarcza zdarzenia z `/dev/input/event*`,
  libxkbcommon mapuje klawisze.
* **drm backend** — używa naszego `/dev/dri/card0` via `libports_drm`.

X11 i XWayland są wyłączone (`-Dxwayland=disabled`).

## Build pipeline

```
tools/wayland_stage.sh         # git clone wszystkich zależności
tools/wayland_build.sh         # meson + ninja w sysroot
tools/wayland_scanner_gen.sh   # wayland-scanner -> build/wayland-protocols/
make build/libports.a          # AMS-OS porting layer
make build/ams-compositor.elf  # kompozytor (wlroots-based)
make build/ams-session.elf     # supervisor uruchamiający kompozytor
```

W razie braku narzędzi (meson, ninja, wayland-scanner, cross-gcc)
skrypty wypisują ostrzeżenie i kończą zerowym kodem — userspace ELFy
nadal się buduje, jedynie nagłówki Wayland są wtedy nieobecne i
kompozytor degraduje się do wariantu loggującego.

## libports (warstwa portu)

`src/libports/`:

| Plik | Rola |
| --- | --- |
| `libports_shm.c`  | `shm_open`/`shm_unlink` → `memfd_create` |
| `libports_poll.c` | `poll`/`epoll_*` → AMS syscalls |
| `libports_unix.c` | `socket`/`bind`/`listen`/`accept`/`connect` |
| `libports_drm.c`  | helpery DRM/GBM (`/dev/dri/card0`) |
| `libports_ffi.c`  | `getauxval`/`sysconf` dla libffi |

`libports.a` jest linkowany do każdego ELF-a Wayland-stacka.

## Mapowanie ABI

| Symbol POSIX        | Implementacja AMS                   |
| ------------------- | ----------------------------------- |
| `shm_open`          | `memfd_create`                      |
| `shm_unlink`        | no-op                               |
| `poll`              | `SYS_POLL`                          |
| `epoll_create1`     | `SYS_EPOLL_CREATE1`                 |
| `epoll_ctl`         | `SYS_EPOLL_CTL`                     |
| `epoll_wait`        | `SYS_EPOLL_WAIT`                    |
| `socket(AF_UNIX,...)` | `SYS_SOCKET`                      |
| `bind`/`listen`/... | bezpośrednie syscalle               |
| `getauxval`         | `libports` (statyczne dane)         |
| `mmap(fd_drm, off)` | `ams_drm_mmap()` w jądrze           |
| `ioctl(drm, ...)`   | `ams_drm_ioctl()` w jądrze          |

## Uruchamianie sesji

```
/dev/dri/card0
/dev/input/event0..N
/run/user/0/wayland-0   <- socket utworzony przez ams-compositor
$WAYLAND_DISPLAY=wayland-0
```

Skrypt startowy: `ams-session` → `ams-compositor` → `wl_display_run`.
