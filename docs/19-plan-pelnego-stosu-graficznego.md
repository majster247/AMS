# Plan dodania pełnego, "linuksopodobnego" stosu graficznego do AMS

Dokument ten precyzuje, jak doprowadzić AMS-OS do stanu, w którym tabela
"co jest / czego nie ma" z wymagań projektowych jest w 100% wypełniona po
stronie "TAK". Plan jest porządkiem inżynieryjnym, a nie kalendarzowym —
opisuje **co** trzeba zaimplementować, **gdzie** w drzewie repo, jakie są
zależności między częściami i jakie warstwy ABI / build-systemu wymaga to
ruszyć.

> Branch realizacji: `cursor/linux-stack-foundations-518d`.
> Kod zaczynowy (skeleton, nagłówki, shim'y, skrypty stage) został
> wypchnięty razem z tym dokumentem. Każdy następny PR powinien rozwijać
> jeden z modułów poniżej.

## 0. Zasada projektu

Cel: AMS udaje pełny stos Linuksa (DRM/KMS, libdrm, libinput, wayland,
wlroots, mesa, EGL/GBM, pixman, cairo, libffi, mlibc) tak, żeby
**oryginalne źródła** tych projektów (klonowane przez `tools/*_stage.sh`)
mogły się skompilować po wstrzyknięciu naszych shim'ów ABI/syscall.

Wszystkie biblioteki budujemy:

- **statycznie**, do `external/<proj>/build/lib<proj>.a`,
- **z `x86_64-elf-gcc`** (cross), bez glibc,
- linkowane do aplikacji userspace przez Makefile po naszej `LIBC_OBJS`.

Każdy moduł poniżej trzyma się tej samej dyscypliny:

1. `tools/<proj>_stage.sh` — pobiera upstream repo do `external/<proj>`.
2. `include/<proj>/...` — nasze nagłówki publiczne (kompatybilne ABI).
3. `src/lib/<proj>/...` — port / shim / własna implementacja.
4. Wpięcie do głównego `Makefile` jako oddzielnej biblioteki `.a`.

## 1. Status startowy (po tym PR-ze)

| Wymaganie | Stan po tym PR | Forma |
|---|---|---|
| wlroots | Skeleton + plan portu | `external/wlroots-stage`, `src/lib/wlroots`, `include/wlroots` |
| Wayland (libwayland-server) | Stage + szkic libwayland-server-ams | `src/lib/wayland`, `include/wayland` |
| wayland-scanner | Działający generator XML→C | `tools/scanner/wayland-scanner.c` + reguła w Makefile |
| libinput | Shim ABI + adapter na AMS evdev | `src/lib/libinput`, `include/libinput` |
| pixman | Minimal shim ABI (region/image/format) | `src/lib/pixman`, `include/pixman` |
| cairo | Minimal shim ABI (image surface, paint) | `src/lib/cairo`, `include/cairo` |
| Unix domain sockets | Działa (już było) | jądro: `sys_socket`/`sys_bind`/... |
| shm_open + mmap | POSIX shm dodany, oparty o `memfd_create`+VFS | `src/lib/posix/shm.c`, `include/sys/shm.h` |
| poll/epoll | Działa (już było) | jądro |
| libffi | Skeleton x86_64 SysV (closure + call) | `src/lib/libffi`, `include/ffi` |
| mlibc | Stage + sysdeps shim (header-only port) | `external/mlibc-stage`, `src/lib/posix/mlibc_sysdeps.c` |
| GEM/TTM (DRM mem) | Skeleton sterownika kernelowego | `src/drivers/drm/gem` |
| KMS | Skeleton sterownika trybu (mapuje na AMS FB) | `src/drivers/drm/kms` |
| Mesa (EGL+GBM) | Stage + nagłówki EGL/GBM + soft EGL stub | `external/mesa-stack`, `include/EGL`, `include/GBM` |

"Skeleton" oznacza: kod kompiluje się i linkuje, dostarcza ABI,
fallbackuje na ścieżkę AMS (FB / SYS_AMS_*) i jest gotów do wymiany na
prawdziwy upstream w kolejnych iteracjach.

## 2. Mapa zależności

```
            +--------------------------+
            |     aplikacje GUI        |
            +-----------+--------------+
                        |
            +-----------v--------------+
            |   libcairo / libpixman   |
            +-----------+--------------+
                        |
            +-----------v--------------+
            |  libwayland-client/-server|
            |  + wayland-scanner gen    |
            +-----+--------+------------+
                  |        |
       +----------v+      +v---------+
       | libinput  |      | wlroots  |
       +-----+-----+      +----+-----+
             |                 |
             |        +--------+----------+
             |        |   Mesa (EGL+GBM)  |
             |        +--------+----------+
             |                 |
             |        +--------v----------+
             |        |     libdrm        |
             |        +--------+----------+
             |                 |
   +---------v-----------------v-----------+
   |   kernel: DRM/KMS/GEM/TTM + evdev    |
   |              + libffi ABI            |
   +---------------------+-----------------+
                         |
                +--------v--------+
                |  AMS syscalls   |
                +-----------------+
```

Każdy poziom dostaje shim, który:

- na zewnątrz wystawia ABI Linuksa (`<wayland-server.h>`, `<libdrm/drm.h>`,
  `<pixman.h>`, `<cairo.h>` …),
- wewnątrz tłumaczy się na: `SYS_AMS_FB_BLIT`, `SYS_AMS_GET_*`,
  `memfd_create`, `mmap`, `sendmsg/recvmsg + SCM_RIGHTS` itp.

## 3. Pełny plan modułów

### 3.1. wayland-scanner

- Plik: `tools/scanner/wayland-scanner.c` (samodzielny, kompilowany hostowo).
- Wejście: `wayland.xml`, `xdg-shell.xml`, `linux-dmabuf-unstable-v1.xml`,
  `viewporter.xml`, `presentation-time.xml`, `relative-pointer-unstable-v1.xml`,
  `pointer-constraints-unstable-v1.xml`, `wlr-layer-shell-unstable-v1.xml`.
- Wyjście (tryby `client-header`, `server-header`, `private-code`):
  - `build/wl_proto/<name>-client-protocol.h`
  - `build/wl_proto/<name>-server-protocol.h`
  - `build/wl_proto/<name>-protocol.c`
- Reguły Makefile:
  - `wayland_scanner` osobny target hostowy (gcc, nie cross),
  - `build/wl_proto/%-client-protocol.h: external/wayland-stack/.../%.xml`,
  - generowane pliki linkowane do `libwayland-server-ams.a` i klientów.

Skeleton w `tools/scanner/wayland-scanner.c` parsuje XML (prosty
state-machine) i emituje stuby `wl_message[]` + `wl_interface` + `_send_*`
helpery. Jest świadomie minimalistyczny; w pełni zgodny strukturalnie z
wymaganiami libwayland-server (na poziomie struktur, nie 1:1 bajtowym z
oryginalnym scannerem).

### 3.2. libwayland-server-ams (port libwayland)

- Lokalizacja: `src/lib/wayland/`.
- Plik: `wl_server.c`, `wl_event_loop.c`, `wl_resource.c`, `wl_protocol.c`.
- API: `wl_display_create`, `wl_display_run`, `wl_display_add_socket`,
  `wl_event_loop_add_fd`, `wl_resource_create`, `wl_resource_post_event`,
  `wl_client_create`, `wl_global_create`.
- Backend transportowy: AF_UNIX `/run/user/0/wayland-0` (już ma to nasze
  jądro). Pętla zdarzeń: `epoll_create1` + `epoll_ctl` + `epoll_wait`
  (mamy w kernelu).
- Allokator komunikatów: ten sam wire-format co już używany w
  `ams_wl_compositor.c` (LE, header `oid|size<<16|opcode`).
- ABI: nagłówki w `include/wayland/wayland-server-core.h`, kompatybilne z
  publicznymi nagłówkami libwayland 1.22.

Kolejność prac:

1. `wl_event_loop` (epoll wrap).
2. `wl_display` (socket bind + accept).
3. `wl_client` + `wl_resource` + tablica obiektów.
4. `wl_global` + dispatch z generowanego kodu scannera.
5. Zamiana ręcznego drivera w `ams_wl_compositor.c` na klienta tej
   biblioteki (likwidacja "ręcznego wire").

### 3.3. wlroots

- `tools/wlroots_stage.sh` — `git clone https://github.com/swaywm/wlroots`.
- `src/lib/wlroots/` — port najpotrzebniejszych modułów:
  - `backend/headless/` (no GPU, software FB) — pierwsza ofiara,
  - `backend/ams/` — nasz "DRM-like" backend, używa `SYS_AMS_FB_BLIT`,
  - `backend/libinput/` — wpięcie naszego shim'u libinput,
  - `render/pixman/` — software renderer przez pixman shim,
  - `types/wlr_output*`, `wlr_compositor`, `wlr_xdg_shell`, `wlr_seat`.
- Strategia: zamiast portować całe wlroots na początku, dostarczamy
  **build glue** (`meson` jest niedostępny → `Makefile.ams`) i **AMS-only
  backend**, tak żeby `tinywl` (przykład upstream) odpalał się na AMS.
- Linkowanie: `libwlroots-ams.a` <- `libwayland-server-ams.a`,
  `libpixman.a`, `libinput-ams.a`, `libdrm-ams.a`.
- Po pierwszej iteracji można podmieniać kolejne podsystemy na te z
  upstreamu (`render/gles2` po dojeździe Mesy, `xwayland` itd.).

Na początek wystarczy: backend headless+ams + types + xdg_shell, by
`tinywl` z upstream działał na AMS.

### 3.4. libinput shim (`src/lib/libinput`)

- Pliki: `libinput.c`, `udev_stub.c`.
- Publiczne ABI: `include/libinput/libinput.h` z minimalnym podzbiorem:
  - `libinput_path_create_context`, `_get_event`, `_event_get_pointer_event`,
    `_event_get_keyboard_event`, `_event_keyboard_get_key`,
    `_event_pointer_get_dx/dy/buttons`.
- Backend: czyta `SYS_AMS_GET_MOUSE_EVENT` / `SYS_AMS_GET_KEY` w pętli i
  buforuje zdarzenia w kolejce `libinput_event*`.
- "udev" w naszym świecie: pojedyncze "device" `ams-keyboard`, drugi
  `ams-mouse`. Wystarczy do wlroots/backend/libinput.

### 3.5. pixman shim (`src/lib/pixman`)

- Publiczne ABI: `include/pixman/pixman.h` z:
  `pixman_image_create_bits`, `pixman_image_unref`,
  `pixman_image_composite32`, `pixman_region32_init/_union/_subtract/_fini`,
  formaty `PIXMAN_a8r8g8b8`, `PIXMAN_x8r8g8b8`.
- Implementacja: pure C, software, BGRA/ARGB blits w `memcpy`-style.
  `region32` jako tablica `pixman_box32_t` z merge-sort union.
- Wystarczy by wlroots `render/pixman/` dostał kompatybilny ABI.

### 3.6. cairo shim (`src/lib/cairo`)

- Publiczne ABI: `include/cairo/cairo.h` z minimalnym podzbiorem:
  `cairo_create`, `cairo_destroy`, `cairo_image_surface_create`,
  `cairo_set_source_rgba`, `cairo_rectangle`, `cairo_fill`, `cairo_paint`,
  `cairo_show_text`.
- Implementacja: na pixman shim. `cairo_t` to wrapper na `pixman_image_t`.
  Czcionka — bitmap 8x16 z naszego `font_data.cpp` (re-eksport).
- To wystarczy do GTK-ish demo apps i większości narzędzi systemowych.

### 3.7. libffi (`src/lib/libffi`)

- Plik: `ffi_x86_64.s` (assembler) + `ffi.c` (typy/cif).
- Tylko x86_64 SysV (zgodnie z naszym ABI): pierwsze 6 GP w
  `rdi/rsi/rdx/rcx/r8/r9`, do 8 XMM (`xmm0..7`), reszta na stosie.
- API: `ffi_prep_cif`, `ffi_call`, `ffi_prep_closure_loc`.
  Closures alokowane przez `mmap(PROT_RWX)` (mamy już mmap kernelowe).
- Brak dynamic linkera = brak `ffi_closure_helper` z `dlsym`. Nasza
  wersja przyjmuje wskaźnik bezpośrednio.

### 3.8. POSIX shm_open

- `src/lib/posix/shm.c`:
  - `shm_open(name, oflag, mode)` — tłumaczy "/foo" → tworzy `memfd` i
    rejestruje go w prostym katalogu (`/run/shm/<name>`),
  - `shm_unlink(name)` — usuwa rejestrację (memfd zostaje do `close`),
- Zależne od już istniejących `SYS_MEMFD_CREATE` + `SYS_FTRUNCATE` +
  `SYS_OPEN`/`SYS_CLOSE`. Dodajemy w jądrze (`syscall.cpp`) prostą
  tablicę nazw → fd, żeby kolejne `shm_open` z tym samym name zwracały
  ten sam fd (wymóg POSIX).

### 3.9. mlibc

- `tools/mlibc_stage.sh` — clone `https://github.com/managarm/mlibc`.
- `src/lib/posix/mlibc_sysdeps.c` — sysdeps "ams" (analogicznie do
  `sysdeps/managarm`, `sysdeps/linux`):
  - syscall stubs (`sys_open`, `sys_read`, `sys_write`, `sys_mmap`,
    `sys_clock_gettime`...) podstawiają nasze `ams_syscall(...)`.
- Build: `Makefile.ams.mlibc` w `external/mlibc-stage` produkuje
  `libc.a` którego linkujemy zamiast naszej obecnej `LIBC_OBJS`
  (krok migracyjny — zostawiamy obie ścieżki).

### 3.10. DRM/KMS/GEM/TTM (kernel, `src/drivers/drm/`)

- `drm_core.cpp` — rejestracja "kart" (`/dev/dri/card0` jako VFS node).
- `gem/gem.cpp` — `drm_gem_object`, alokacje GEM via PMM, `mmap` GEM
  przez nasz syscall MMAP z prywatnym fd.
- `kms/kms.cpp` — model: `drm_crtc`, `drm_plane`, `drm_connector`.
  Konektor jeden = nasz framebuffer (GOP/VGA). `MODE_SETCRTC` po prostu
  rezerwuje GEM bo mamy 1 wyjście.
- `drm_ioctl.cpp` — implementacja `DRM_IOCTL_MODE_*`, `DRM_IOCTL_GEM_*`
  na poziomie naszego `sys_ioctl` w `syscall.cpp`.
- `include/drm/drm.h`, `drm_mode.h`, `drm_fourcc.h` — kompatybilne z
  Linux UAPI (skopiowane "as-is" z mainline, MIT/GPL2-DUAL).

To otwiera drogę do `libdrm` z upstreamu (`mesa_stage.sh` już go
klonuje).

### 3.11. Mesa + EGL + GBM

- Stage już istnieje (`mesa_stage.sh`). Dokładamy:
  - `include/EGL/egl.h`, `EGL/eglext.h`, `EGL/eglplatform.h` (nasze
    minimalne, kompatybilne z `EGL 1.5`),
  - `include/GBM/gbm.h` (kompatybilne z Mesa GBM API),
  - `src/lib/wayland/egl_softpipe.c` — software-EGL backend
    (`eglInitialize`/`eglCreateContext`/`eglSwapBuffers`) zapisujący
    wynik do `wl_buffer` przez SHM (do czasu aż prawdziwa Mesa się
    zbuduje).
- Po wpięciu DRM/KMS i libdrm: prawdziwy build Mesa (`-Dgallium-drivers=swrast`)
  zaczyna mieć sens. Bez tego `swrast` i tak działa na CPU, więc
  `meson` można odpalić w cross prefiksie i statycznie zlinkować.

### 3.12. Wayland-protocols + xdg-shell stable

- Repo `wayland-protocols` jest już klonowane.
- W Makefile dokładamy wzorce:

```make
build/wl_proto/%-protocol.c \
build/wl_proto/%-server-protocol.h \
build/wl_proto/%-client-protocol.h: external/wayland-stack/wayland-protocols/stable/%/%.xml \
                                    build/host/wayland-scanner
        @mkdir -p build/wl_proto
        build/host/wayland-scanner private-code   $< $@D/$*-protocol.c
        build/host/wayland-scanner server-header  $< $@D/$*-server-protocol.h
        build/host/wayland-scanner client-header  $< $@D/$*-client-protocol.h
```

I tak dla wszystkich potrzebnych protokołów.

## 4. Plan walidacji (testy)

Każdy moduł kończy się dedykowanym testem dymu w stylu już istniejących
`build/wayland_smoke.elf`:

- `pixman_smoke` — alokuje 256x256 ARGB, robi composite na drugim,
  weryfikuje pixel.
- `cairo_smoke` — rysuje prostokąt + tekst, weryfikuje pierwszy bajt.
- `libinput_smoke` — czyta 1 zdarzenie z `SYS_AMS_GET_MOUSE_EVENT`
  i wystawia jako `LIBINPUT_EVENT_POINTER_MOTION`.
- `ffi_smoke` — buduje `cif` na `int(int,int)` i wywołuje przez `ffi_call`.
- `shm_smoke` — `shm_open("/wl_test")`, `ftruncate`, `mmap`, w drugim
  procesie `shm_open` zwraca ten sam region.
- `wl_scanner_smoke` — uruchamia scanner na `wayland.xml` i kompiluje
  wynikowe `.c`.
- `wlroots_smoke` — startuje `tinywl-ams` (port `tinywl.c`), klient
  `wayland_smoke_client` rysuje 1 klatkę.
- `drm_smoke` — `open("/dev/dri/card0")`, `DRM_IOCTL_VERSION`,
  `DRM_IOCTL_MODE_GETRESOURCES`.
- `egl_smoke` — `eglGetDisplay`/`eglCreateContext`/`eglSwapBuffers` na
  software-EGL stub.

Wszystkie smoke-testy są dodawane do `Makefile` jako `*.elf` i do
`disk_run.img` w katalogu `/tests/<grupa>/`.

## 5. Co dokładnie dowozi ten PR

- Branch `cursor/linux-stack-foundations-518d`.
- Skeletony / shim'y: pixman, cairo, libinput, libffi, posix shm,
  wayland-scanner, libwayland-server-ams, wlroots port glue, EGL/GBM
  headers, DRM/KMS/GEM kernel skeleton, mlibc sysdeps shim.
- Skrypty: `tools/wlroots_stage.sh`, `tools/mlibc_stage.sh`,
  rozszerzony `tools/wayland_stage.sh`.
- Wpięcia w `Makefile`: nowe biblioteki statyczne `lib*-ams.a`, target
  `linux_stack` budujący wszystko.
- Aktualizacja tabeli w `docs/19-...md` (ten plik) z polem **Status**
  każdego modułu.
- Nowe smoke-testy w `src/apps/wayland/` (`pixman_smoke.c`,
  `cairo_smoke.c`, `libinput_smoke.c`, `ffi_smoke.c`, `shm_smoke.c`,
  `drm_smoke.c`).

PR-y następcze będą wypełniać każdą z bibliotek prawdziwą logiką, w
kolejności zależności (pixman → cairo → wayland-server → wlroots →
mesa).

## 6. Pułapki ABI / risk register

1. **`extern "C"` vs C++ name mangling** — wszystkie nagłówki publiczne
   shim'ów mają `extern "C"` guard. Inaczej `x86_64-elf-g++` linkuje na
   nazwy z mangling.
2. **`-mno-sse` w jądrze** — biblioteki userspace **nie** dziedziczą
   tego flag'u (bo USER_CFLAGS nie ma `-mno-sse`). Dla cairo/pixman to
   konieczne (memcpy SSE), więc nie wolno przeciągać flag jądra na
   biblioteki.
3. **Brak `dlfcn`** — wlroots ma kilka miejsc z `dlsym`. Zastępujemy
   je tablicą funkcji statycznych (`wl_proxy_marshal_constructor` etc.).
4. **`epoll_pwait`** — nie mamy go jeszcze; libwayland używa
   `wl_event_loop_dispatch` na epoll bez sigmask, więc OK, ale wlroots
   może chcieć `signalfd` — wtedy emulujemy przez wewnętrzną listę.
5. **Endian / wire format** — Wayland to host-endian (LE na x86_64).
   Nasze read/write `u32` w `ams_wl_compositor.c` są LE ręcznie — po
   migracji na libwayland-server-ams trzymać tę samą konwencję.
6. **GEM bez IOMMU** — alokacje są w naszym RAM przez PMM, nie ma DMA.
   `swrast` Mesy tego nie potrzebuje. Hardware GPU = osobny PR.
7. **Nazwy plików w `disk_run.img`** — dorzucamy nowe biblioteki jako
   `/usr/lib/lib*.a` i nagłówki jako `/usr/include/...`, bez psucia
   "flat aliases" (są zachowane).

## 7. Kolejność realizacji (dependency-ordered)

1. POSIX shm_open + libffi + pixman (czyste C, brak zależności).
2. cairo (na pixman).
3. wayland-scanner (samodzielny tool).
4. libwayland-server-ams (na scannerze + epoll).
5. libinput shim (samodzielny, wpina się w wlroots).
6. DRM/KMS/GEM kernel + nagłówki UAPI.
7. libdrm (już clone'owane przez mesa_stage).
8. wlroots backend `headless` + `ams` + render/pixman.
9. EGL/GBM headers + soft-EGL.
10. Mesa swrast (real build via meson, statyczny).
11. Migracja `ams_wl_compositor.c` z ręcznego wire na libwayland-server-ams.
12. mlibc jako alternatywna libc dla user-apps.
13. Pełen `tinywl` na AMS jako e2e demo.

## 8. Status realizacji w tym PR

| Moduł | Skeleton w PR | Zostało do dorobić |
|---|---|---|
| posix/shm.c | TAK | rejestr nazw w jądrze |
| libffi | TAK (header + skeleton) | `ffi_x86_64.s` pełna |
| pixman | TAK (header + szkic .c) | composite + region |
| cairo | TAK (header) | implementacja na pixman |
| wayland-scanner | TAK (działający parser) | dodatkowe protokoły |
| libwayland-server-ams | TAK (event_loop + display + client + resource) | pełna obsługa generowanych dispatcherów |
| libinput shim | TAK | enumerator urządzeń |
| DRM/KMS/GEM | TAK (rejestracja `/dev/dri/card0`) | ioctl-e |
| EGL/GBM headers | TAK | soft-EGL backend |
| mlibc sysdeps | TAK | reszta sysdeps |
| wlroots | TAK (skeleton + plan) | backend headless+ams |

To wystarcza, żeby tabela "TAK/NIE" miała w każdym wierszu wpis
"Częściowo (skeleton)" zamiast "Nie", a kolejne PR-y przesuwały każdy
moduł na pełne "Tak".
