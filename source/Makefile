# --- Ścieżki i Narzędzia ---
export PATH := $(HOME)/opt/cross/bin:$(PATH)

CC = x86_64-elf-g++
AS = nasm
LD = x86_64-elf-ld
ISO_IMAGE = ams_run.iso
DISK_IMAGE = disk_run.img
DISK_SIZE ?= 4G
WAYLAND_STAGE_DIR = external/wayland-stack
WAYLAND_AMS_SRC_DIR = $(WAYLAND_STAGE_DIR)/ams-src

# --- Flagi Jądra (Ring 0) - PANCERNE (Bez SSE, Bez RedZone) ---
CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -Iinclude \
         -fno-exceptions -fno-rtti -fno-stack-protector \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -DKERNEL_MODE

ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld

# --- Flagi Aplikacji (Ring 3) ---
# TCC i Hello mogą używać SSE, ale muszą mieć biblioteki.
USER_CFLAGS = -m64 -ffreestanding -O2 -Wall -Iinclude -fno-exceptions -fno-rtti -fno-stack-protector
USER_LDFLAGS = -n -T src/gui/apps/user.ld

# =============================================================================
# PLIKI
# =============================================================================

# 1. Wszystkie pliki .cpp
SRCS_ALL_CPP := $(shell find src -name '*.cpp')
SRCS_USER_LIB := $(shell find src/lib -name '*.cpp')

# 2. Pliki JĄDRA
#    Wyrzucamy wszystko co jest w src/lib oraz hello_user.
#    Jądro używa WŁASNYCH wersji z utils.cpp i heap.cpp
SRCS_KERNEL_CPP := $(filter-out src/gui/apps/hello_user.cpp $(SRCS_USER_LIB), $(SRCS_ALL_CPP))

# 3. ASM Jądra
SRCS_KERNEL_ASM := $(shell find src -name '*.s' ! -path "src/lib/*")

# 4. Obiekty Jądra
KERNEL_OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS_KERNEL_CPP))
KERNEL_OBJS += $(patsubst src/%.s, build/%.o, $(SRCS_KERNEL_ASM))

# 5. Obiekty Biblioteki Użytkownika (do linkowania z TCC i Hello)
#    Te pliki będą kompilowane flagami USER_CFLAGS (albo bezpiecznymi, zależy od reguły)
LIBC_OBJS := build/lib/crt0.o \
             build/lib/syscall.o \
             build/lib/ams_syscall.o \
             build/lib/stdlib.o \
             build/lib/string.o \
             build/lib/ctype.o \
             build/lib/stdio.o \
             build/lib/stubs.o \
             build/lib/setjmp.o

# =============================================================================
# CELE
# =============================================================================
all: kernel.elf build/hello.elf build/tcc.elf build/doom.elf \
     build/libports.a \
     build/ams-compositor.elf build/ams-session.elf \
     build/ams-smoke-client.elf build/ams-egl-smoke.elf \
     build/apt.elf build/jobctl.elf build/gcc.elf build/bash.elf \
     $(ISO_IMAGE) $(DISK_IMAGE)

# --- KOMPILACJA JĄDRA ---

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# LINKOWANIE JĄDRA
# UWAGA: Usunąłem $(LIBC_OBJS) stąd! Jądro jest samowystarczalne.
kernel.elf: $(KERNEL_OBJS)
	@echo "[LINK] Budowanie Jądra..."
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

# --- KOMPILACJA BIBLIOTEKI USERA ---

# Biblioteki kompilujemy BEZPIECZNIE (CFLAGS), żeby uniknąć syfu z SSE w dziwnych miejscach.
# Ale jeśli TCC potrzebuje floatów, to w stdlib.cpp masz #ifdef __SSE__.
# Tutaj użyjemy USER_CFLAGS, żeby TCC miało floaty, ale Jądro i tak korzysta z utils.cpp.

build/lib/%.o: src/lib/%.cpp
	@mkdir -p build/lib
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/lib/%.o: src/lib/%.s
	@mkdir -p build/lib
	$(AS) $(ASFLAGS) $< -o $@

build/lib/%.o: src/lib/%.c
	@mkdir -p build/lib
	x86_64-elf-gcc $(USER_CFLAGS) -c $< -o $@

# --- KOMPILACJA USER APPS ---

# Hello User
build/apps/hello_user.o: src/gui/apps/hello_user.cpp
	@mkdir -p build/apps
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/hello.elf: build/apps/hello_user.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie hello.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/hello_user.o $(LIBC_OBJS) -o build/hello.elf

# TCC
build/apps/tcc.o: src/apps/tcc/tcc.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -w -DONE_SOURCE=1 -DTCC_TARGET_X86_64 -DHAVE_SYS_MMAP=1\
	-nostdinc -Iinclude -isystem include \
	-Isrc/apps/tcc \
	-c src/apps/tcc/tcc.c -o build/apps/tcc.o

build/tcc.elf: build/apps/tcc.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie TCC..."
	$(LD) $(USER_LDFLAGS) build/apps/tcc.o $(LIBC_OBJS) -o build/tcc.elf

# Doomgeneric (real sources from GitHub repo in external/doomgeneric)
DOOM_CFLAGS = $(USER_CFLAGS) -std=gnu99 -w -DDOOMGENERIC -D_DEFAULT_SOURCE \
              -Iexternal/doomgeneric/doomgeneric -Isrc/apps/doomgeneric

DOOM_SRC = \
	external/doomgeneric/doomgeneric/dummy.c \
	external/doomgeneric/doomgeneric/am_map.c \
	external/doomgeneric/doomgeneric/doomdef.c \
	external/doomgeneric/doomgeneric/doomstat.c \
	external/doomgeneric/doomgeneric/dstrings.c \
	external/doomgeneric/doomgeneric/d_event.c \
	external/doomgeneric/doomgeneric/d_iwad.c \
	external/doomgeneric/doomgeneric/d_items.c \
	external/doomgeneric/doomgeneric/d_loop.c \
	external/doomgeneric/doomgeneric/d_main.c \
	external/doomgeneric/doomgeneric/d_mode.c \
	external/doomgeneric/doomgeneric/d_net.c \
	external/doomgeneric/doomgeneric/f_finale.c \
	external/doomgeneric/doomgeneric/f_wipe.c \
	external/doomgeneric/doomgeneric/g_game.c \
	external/doomgeneric/doomgeneric/hu_lib.c \
	external/doomgeneric/doomgeneric/hu_stuff.c \
	external/doomgeneric/doomgeneric/info.c \
	external/doomgeneric/doomgeneric/i_cdmus.c \
	external/doomgeneric/doomgeneric/i_endoom.c \
	external/doomgeneric/doomgeneric/i_joystick.c \
	external/doomgeneric/doomgeneric/i_scale.c \
	external/doomgeneric/doomgeneric/i_sound.c \
	external/doomgeneric/doomgeneric/i_system.c \
	external/doomgeneric/doomgeneric/i_timer.c \
	external/doomgeneric/doomgeneric/memio.c \
	external/doomgeneric/doomgeneric/m_argv.c \
	external/doomgeneric/doomgeneric/m_bbox.c \
	external/doomgeneric/doomgeneric/m_cheat.c \
	external/doomgeneric/doomgeneric/m_config.c \
	external/doomgeneric/doomgeneric/m_controls.c \
	external/doomgeneric/doomgeneric/m_fixed.c \
	external/doomgeneric/doomgeneric/m_menu.c \
	external/doomgeneric/doomgeneric/m_misc.c \
	external/doomgeneric/doomgeneric/m_random.c \
	external/doomgeneric/doomgeneric/p_ceilng.c \
	external/doomgeneric/doomgeneric/p_doors.c \
	external/doomgeneric/doomgeneric/p_enemy.c \
	external/doomgeneric/doomgeneric/p_floor.c \
	external/doomgeneric/doomgeneric/p_inter.c \
	external/doomgeneric/doomgeneric/p_lights.c \
	external/doomgeneric/doomgeneric/p_map.c \
	external/doomgeneric/doomgeneric/p_maputl.c \
	external/doomgeneric/doomgeneric/p_mobj.c \
	external/doomgeneric/doomgeneric/p_plats.c \
	external/doomgeneric/doomgeneric/p_pspr.c \
	external/doomgeneric/doomgeneric/p_saveg.c \
	external/doomgeneric/doomgeneric/p_setup.c \
	external/doomgeneric/doomgeneric/p_sight.c \
	external/doomgeneric/doomgeneric/p_spec.c \
	external/doomgeneric/doomgeneric/p_switch.c \
	external/doomgeneric/doomgeneric/p_telept.c \
	external/doomgeneric/doomgeneric/p_tick.c \
	external/doomgeneric/doomgeneric/p_user.c \
	external/doomgeneric/doomgeneric/r_bsp.c \
	external/doomgeneric/doomgeneric/r_data.c \
	external/doomgeneric/doomgeneric/r_draw.c \
	external/doomgeneric/doomgeneric/r_main.c \
	external/doomgeneric/doomgeneric/r_plane.c \
	external/doomgeneric/doomgeneric/r_segs.c \
	external/doomgeneric/doomgeneric/r_sky.c \
	external/doomgeneric/doomgeneric/r_things.c \
	external/doomgeneric/doomgeneric/sha1.c \
	external/doomgeneric/doomgeneric/sounds.c \
	external/doomgeneric/doomgeneric/statdump.c \
	external/doomgeneric/doomgeneric/st_lib.c \
	external/doomgeneric/doomgeneric/st_stuff.c \
	external/doomgeneric/doomgeneric/s_sound.c \
	external/doomgeneric/doomgeneric/tables.c \
	external/doomgeneric/doomgeneric/v_video.c \
	external/doomgeneric/doomgeneric/wi_stuff.c \
	external/doomgeneric/doomgeneric/w_checksum.c \
	external/doomgeneric/doomgeneric/w_file.c \
	external/doomgeneric/doomgeneric/w_main.c \
	external/doomgeneric/doomgeneric/w_wad.c \
	external/doomgeneric/doomgeneric/z_zone.c \
	external/doomgeneric/doomgeneric/w_file_stdc.c \
	external/doomgeneric/doomgeneric/i_input.c \
	external/doomgeneric/doomgeneric/i_video.c \
	external/doomgeneric/doomgeneric/doomgeneric.c \
	src/apps/doomgeneric/doomgeneric_ams.c

DOOM_OBJS = $(patsubst %.c,build/doom/%.o,$(DOOM_SRC))

build/doom/%.o: %.c
	@mkdir -p $(dir $@)
	x86_64-elf-gcc $(DOOM_CFLAGS) -c $< -o $@

build/doom.elf: $(DOOM_OBJS) $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie doom.elf (doomgeneric)..."
	$(LD) $(USER_LDFLAGS) $(DOOM_OBJS) $(LIBC_OBJS) -o build/doom.elf

# =============================================================================
# Wayland desktop graphics stack
# =============================================================================
#
# tools/wayland_stage.sh fetches the upstream sources (libwayland,
# wayland-protocols, libxkbcommon, libinput, pixman, cairo, libffi,
# mlibc, mesa, wlroots) into external/wayland-stack/. tools/wayland_build.sh
# then drives meson/ninja to install them into a sysroot which the
# AMS-OS user-space ELFs link against.
WAYLAND_SYSROOT := $(WAYLAND_STAGE_DIR)/sysroot
WAYLAND_PROTO_OUT := build/wayland-protocols

wayland_stage:
	@echo "[WAYLAND] Cloning/updating upstream sources (mlibc, libffi, pixman, cairo,"
	@echo "[WAYLAND]  libxkbcommon, libinput, wayland, wayland-protocols, mesa, wlroots)..."
	@bash tools/wayland_stage.sh

wayland_sync_apps: wayland_stage
	@echo "[WAYLAND] Syncing AMS Wayland app sources into stage..."
	@bash tools/wayland_sync_apps.sh

wayland_scanner_gen: wayland_stage
	@echo "[WAYLAND] Generating protocol code via wayland-scanner..."
	@bash tools/wayland_scanner_gen.sh

wayland_build: wayland_stage
	@echo "[WAYLAND] Building libffi/wayland/protocols/xkbcommon/libinput/pixman/cairo/mesa/wlroots..."
	@bash tools/wayland_build.sh

wayland_desktop_matrix: build/ams-compositor.elf build/ams-session.elf build/ams-smoke-client.elf build/ams-egl-smoke.elf
	@echo "[WAYLAND] Running desktop compatibility matrix..."
	@bash tools/wayland_desktop_matrix.sh

# --- libports - AMS-OS porting layer (shm_open, poll, epoll, AF_UNIX, DRM) ---
LIBPORTS_SRCS := \
	src/libports/libports_shm.c \
	src/libports/libports_poll.c \
	src/libports/libports_unix.c \
	src/libports/libports_drm.c \
	src/libports/libports_ffi.c

LIBPORTS_OBJS := $(patsubst src/%.c,build/%.o,$(LIBPORTS_SRCS))

build/libports/%.o: src/libports/%.c
	@mkdir -p build/libports
	x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -Iinclude/libports -c $< -o $@

build/libports.a: $(LIBPORTS_OBJS)
	@echo "[AR]   Budowanie libports.a..."
	x86_64-elf-ar rcs $@ $(LIBPORTS_OBJS)

# --- Compositor (wlroots-based) ---
COMPOSITOR_CFLAGS := $(USER_CFLAGS) -std=gnu99 \
	-Iinclude -Iinclude/libports \
	-I$(WAYLAND_SYSROOT)/usr/include \
	-I$(WAYLAND_PROTO_OUT) \
	-DWLR_USE_UNSTABLE=1
COMPOSITOR_LIBS := -L$(WAYLAND_SYSROOT)/usr/lib \
	-lwlroots -lwayland-server -lwayland-client -lwayland-cursor \
	-lxkbcommon -linput -lpixman-1 -lcairo -lEGL -lGLESv2 -lgbm -lffi

build/apps/ams_compositor.o: src/apps/wayland/ams_compositor.c | wayland_sync_apps
	@mkdir -p build/apps
	x86_64-elf-gcc $(COMPOSITOR_CFLAGS) -c $< -o $@ || \
	  x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -Iinclude/libports -c $< -o $@

build/ams-compositor.elf: build/apps/ams_compositor.o build/libports.a $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie ams-compositor.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/ams_compositor.o build/libports.a $(LIBC_OBJS) -o build/ams-compositor.elf

# --- Session supervisor ---
build/apps/ams_session.o: src/apps/wayland/ams_session.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -c $< -o $@

build/ams-session.elf: build/apps/ams_session.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie ams-session.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/ams_session.o $(LIBC_OBJS) -o build/ams-session.elf

# --- Wayland client smoke test (libwayland-client based) ---
build/apps/ams_smoke_client.o: src/apps/wayland/ams_smoke_client.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(COMPOSITOR_CFLAGS) -c $< -o $@ || \
	  x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -Iinclude/libports -c $< -o $@

build/ams-smoke-client.elf: build/apps/ams_smoke_client.o build/libports.a $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie ams-smoke-client.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/ams_smoke_client.o build/libports.a $(LIBC_OBJS) -o build/ams-smoke-client.elf

# --- EGL/GBM smoke test ---
build/apps/ams_egl_smoke.o: src/apps/wayland/ams_egl_smoke.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(COMPOSITOR_CFLAGS) -c $< -o $@ || \
	  x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -Iinclude/libports -c $< -o $@

build/ams-egl-smoke.elf: build/apps/ams_egl_smoke.o build/libports.a $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie ams-egl-smoke.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/ams_egl_smoke.o build/libports.a $(LIBC_OBJS) -o build/ams-egl-smoke.elf

build/apps/apt.o: src/apps/pkg/apt.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -c $< -o $@

build/apt.elf: build/apps/apt.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie apt.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/apt.o $(LIBC_OBJS) -o build/apt.elf

build/apps/jobctl.o: src/apps/tools/jobctl.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -c $< -o $@

build/jobctl.elf: build/apps/jobctl.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie jobctl.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/jobctl.o $(LIBC_OBJS) -o build/jobctl.elf

build/apps/gcc.o: src/apps/tools/gcc.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -c $< -o $@

build/gcc.elf: build/apps/gcc.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie gcc.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/gcc.o $(LIBC_OBJS) -o build/gcc.elf

build/apps/bash.o: src/apps/tools/bash.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -std=gnu99 -c $< -o $@

build/bash.elf: build/apps/bash.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie bash.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/bash.o $(LIBC_OBJS) -o build/bash.elf

# --- OBRAZY ---
$(DISK_IMAGE): build/tcc.elf build/doom.elf \
               build/ams-compositor.elf build/ams-session.elf \
               build/ams-smoke-client.elf build/ams-egl-smoke.elf \
               build/apt.elf build/jobctl.elf build/gcc.elf build/bash.elf
	@echo "[DISK] Budowanie srodowiska testowego..."
	qemu-img create -f raw $(DISK_IMAGE) $(DISK_SIZE)
	mkfs.ext2 -b 4096 $(DISK_IMAGE)
	mkdir -p ./temp_mount
	sudo mount -o loop $(DISK_IMAGE) ./temp_mount
	
	# Docelowa struktura systemu
	sudo mkdir -p ./temp_mount/programs/doom
	sudo mkdir -p ./temp_mount/programs/wayland
	sudo mkdir -p ./temp_mount/tools/system
	sudo mkdir -p ./temp_mount/tools/compiler/include
	sudo mkdir -p ./temp_mount/tools/compiler/lib
	sudo mkdir -p ./temp_mount/tools/compiler/runtime
	sudo mkdir -p ./temp_mount/usr/local/lib/tcc/include
	sudo mkdir -p ./temp_mount/usr/local/lib/tcc/lib
	sudo mkdir -p ./temp_mount/tools/toolchain/bin
	sudo mkdir -p ./temp_mount/tools/toolchain/lib
	sudo mkdir -p ./temp_mount/tools/toolchain/meta
	sudo mkdir -p ./temp_mount/programs/wayland/mesa
	sudo mkdir -p ./temp_mount/tests/tcc
	sudo mkdir -p ./temp_mount/tests/toolchain

	# Programy
	sudo cp build/doom.elf ./temp_mount/programs/doom/doom.base.elf
	sudo cp src/apps/doomgeneric/doom_bootstrap.c ./temp_mount/programs/doom/doom_bootstrap.c
	sudo cp freedoom-0.13.0/freedoom1.wad ./temp_mount/programs/doom/freedoom1.wad
	sudo cp build/ams-compositor.elf   ./temp_mount/programs/wayland/ams-compositor
	sudo cp build/ams-session.elf      ./temp_mount/programs/wayland/ams-session
	sudo cp build/ams-smoke-client.elf ./temp_mount/programs/wayland/ams-smoke-client
	sudo cp build/ams-egl-smoke.elf    ./temp_mount/programs/wayland/ams-egl-smoke
	sudo mkdir -p ./temp_mount/run/user/0
	sudo mkdir -p ./temp_mount/dev/dri ./temp_mount/dev/input

	# Narzędzia systemowe
	sudo cp build/apt.elf ./temp_mount/tools/system/apt
	sudo cp build/jobctl.elf ./temp_mount/tools/system/jobctl
	sudo cp build/bash.elf ./temp_mount/tools/system/bash

	# Kompilator i runtime
	sudo cp build/tcc.elf ./temp_mount/tools/compiler/tcc
	sudo cp build/gcc.elf ./temp_mount/tools/compiler/gcc
	sudo cp include/ams_syscall.h ./temp_mount/tools/compiler/include/ams_syscall.h
	sudo cp tinycc-mirror-repository/include/tccdefs.h ./temp_mount/tools/compiler/include/tccdefs.h
	sudo cp libtcc1.a ./temp_mount/tools/compiler/lib/libtcc1.a
	sudo cp build/lib/crt0.o ./temp_mount/tools/compiler/runtime/crt0.o
	sudo cp build/lib/syscall.o ./temp_mount/tools/compiler/runtime/syscall.o
	sudo cp build/lib/ams_syscall.o ./temp_mount/tools/compiler/runtime/ams_syscall.o
	sudo cp build/lib/stdlib.o ./temp_mount/tools/compiler/runtime/stdlib.o
	sudo cp build/lib/string.o ./temp_mount/tools/compiler/runtime/string.o
	sudo cp build/lib/ctype.o ./temp_mount/tools/compiler/runtime/ctype.o
	sudo cp include/ams_syscall.h ./temp_mount/usr/local/lib/tcc/include/ams_syscall.h
	sudo cp tinycc-mirror-repository/include/tccdefs.h ./temp_mount/usr/local/lib/tcc/include/tccdefs.h
	sudo cp build/lib/crt0.o ./temp_mount/usr/local/lib/tcc/lib/crt0.o
	sudo cp build/lib/ams_syscall.o ./temp_mount/usr/local/lib/tcc/lib/ams_syscall.o

	# Host-built toolchain payload copied INTO AMS disk (for in-system staging/testing)
	@if [ -d external/toolchain/prefix/bin ]; then \
		echo "[DISK] Copying host-built binutils+gcc payload into AMS image..."; \
		sudo cp -a external/toolchain/prefix/bin/. ./temp_mount/tools/toolchain/bin/; \
		if [ -d external/toolchain/prefix/lib ]; then sudo cp -a external/toolchain/prefix/lib/. ./temp_mount/tools/toolchain/lib/; fi; \
		if [ -d external/toolchain/prefix/libexec ]; then sudo mkdir -p ./temp_mount/tools/toolchain/libexec && sudo cp -a external/toolchain/prefix/libexec/. ./temp_mount/tools/toolchain/libexec/; fi; \
		if [ -d external/toolchain/prefix/x86_64-elf ]; then sudo mkdir -p ./temp_mount/tools/toolchain/x86_64-elf && sudo cp -a external/toolchain/prefix/x86_64-elf/. ./temp_mount/tools/toolchain/x86_64-elf/; fi; \
		echo "x86_64-elf-gcc copied from host stage" | sudo tee ./temp_mount/tools/toolchain/meta/status.txt >/dev/null; \
	else \
		echo "toolchain stage not found on host" | sudo tee ./temp_mount/tools/toolchain/meta/status.txt >/dev/null; \
	fi

	# Mesa payload copied INTO AMS disk (software-first data/staging artifacts)
	@if [ -d external/mesa-stack/mesa/.git ]; then \
		echo "[DISK] Copying Mesa stage metadata into AMS image..."; \
		git -C external/mesa-stack/mesa rev-parse --short HEAD | sudo tee ./temp_mount/programs/wayland/mesa/mesa_commit.txt >/dev/null; \
	fi
	@if [ -d $(WAYLAND_STAGE_DIR)/wayland/.git ]; then \
		echo "[DISK] Copying Wayland stage metadata into AMS image..."; \
		git -C $(WAYLAND_STAGE_DIR)/wayland rev-parse --short HEAD | sudo tee ./temp_mount/programs/wayland/mesa/wayland_commit.txt >/dev/null; \
	fi
	@if [ -d $(WAYLAND_STAGE_DIR)/wayland-protocols/.git ]; then \
		git -C $(WAYLAND_STAGE_DIR)/wayland-protocols rev-parse --short HEAD | sudo tee ./temp_mount/programs/wayland/mesa/wayland_protocols_commit.txt >/dev/null; \
	fi
	@if [ -d external/mesa-stack/build-mesa-soft ]; then \
		echo "[DISK] Copying Mesa software build artifacts..."; \
		sudo cp -a external/mesa-stack/build-mesa-soft/. ./temp_mount/programs/wayland/mesa/; \
	else \
		echo "mesa local build artifacts missing (meson/ninja not available during stage)" | sudo tee ./temp_mount/programs/wayland/mesa/status.txt >/dev/null; \
	fi

	# Flat aliases (zgodność wsteczna)
	sudo cp include/ams_syscall.h ./temp_mount/ams_syscall.h
	sudo cp build/lib/crt0.o ./temp_mount/crt0.o
	sudo cp build/lib/syscall.o ./temp_mount/syscall.o
	sudo cp build/lib/ams_syscall.o ./temp_mount/ams_syscall.o
	sudo cp build/lib/stdlib.o ./temp_mount/stdlib.o
	sudo cp build/lib/string.o ./temp_mount/string.o
	sudo cp build/lib/ctype.o ./temp_mount/ctype.o
	sudo cp build/tcc.elf ./temp_mount/tcc
	sudo cp build/gcc.elf ./temp_mount/gcc
	sudo cp build/bash.elf ./temp_mount/bash
	sudo cp build/jobctl.elf ./temp_mount/jobctl
	sudo cp build/apt.elf ./temp_mount/apt
	sudo cp tinycc-mirror-repository/include/tccdefs.h ./temp_mount/tccdefs.h
	sudo cp src/apps/doomgeneric/doom_bootstrap.c ./temp_mount/doom_bootstrap.c
	sudo cp build/ams-compositor.elf    ./temp_mount/ams-compositor
	sudo cp build/ams-session.elf       ./temp_mount/ams-session
	sudo cp build/ams-smoke-client.elf  ./temp_mount/ams-smoke-client
	sudo cp build/ams-egl-smoke.elf     ./temp_mount/ams-egl-smoke
	sudo cp build/doom.elf ./temp_mount/doom.base.elf
	sudo cp freedoom-0.13.0/freedoom1.wad ./temp_mount/freedoom1.wad
	
	# --- GENEROWANIE 10 PLIKÓW TESTOWYCH ---
	@echo "int main() { return 123; }" | sudo tee ./temp_mount/tests/tcc/t1_basic.c > /dev/null
	@echo "int f(int n){if(n<2)return n; return f(n-1)+f(n-2);} int main(){ return f(7); }" | sudo tee ./temp_mount/tests/tcc/t2_fib.c > /dev/null
	@echo "int main(){ int a=10; int* p=&a; *p=42; return a; }" | sudo tee ./temp_mount/tests/tcc/t3_ptr.c > /dev/null
	@echo "struct S{int x; int y;}; int main(){ struct S s; s.x=10; s.y=20; return s.x+s.y; }" | sudo tee ./temp_mount/tests/tcc/t4_struct.c > /dev/null
	@echo "int main(){ int s=0; for(int i=0; i<100; i++) s+=i; return s; }" | sudo tee ./temp_mount/tests/tcc/t5_loops.c > /dev/null
	@echo "#include \"/tools/compiler/include/tccdefs.h\"\nint main(){ return 0; }" | sudo tee ./temp_mount/tests/tcc/t6_include.c > /dev/null
	@echo "int main(){ float a=1.5f; float b=2.5f; return (int)(a+b+5.0f); }" | sudo tee ./temp_mount/tests/tcc/t7_math.c > /dev/null
	@echo "int main(){ char msg[]=\"AMS\"; if(msg[0]=='A') return 1; return 0; }" | sudo tee ./temp_mount/tests/tcc/t8_strings.c > /dev/null
	@echo "int main(){ unsigned int x=0x12345678; return (x >> 24) & 0xFF; }" | sudo tee ./temp_mount/tests/tcc/t9_bit.c > /dev/null
	@echo "void exit(int); int main(){ exit(0); return 99; }" | sudo tee ./temp_mount/tests/tcc/t10_io.c > /dev/null
	sudo cp src/apps/tcc/kupa.c ./temp_mount/tests/tcc/kupa.c
	@echo "int main(){ return 0; }" | sudo tee ./temp_mount/tests/toolchain/hello.c > /dev/null
	
	sudo sync
	sudo umount -d ./temp_mount
	@echo "[DISK] Obraz $(DISK_IMAGE) z 10 testami gotowy."

$(ISO_IMAGE): kernel.elf
	@echo "[ISO] Tworzenie obrazu bootowalnego..."
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/
	echo "AMSx64 v1.2.5 ring 3 version" > hello2.txt
	tar -cf initrd.tar hello2.txt bible.txt
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "AMSx64 v1.2.5 ring 3 version" {\n multiboot2 /boot/kernel.elf\n module2 /boot/initrd.tar\n boot\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) iso

run: all
	qemu-system-x86_64 -m 8G -cdrom $(ISO_IMAGE) -serial stdio \
        -drive id=disk,file=$(DISK_IMAGE),if=none,format=raw,readonly=off \
        -device ich9-ahci,id=ahci \
        -device ide-hd,drive=disk,bus=ahci.0 \
        -vga std \
        -display gtk \
		-d int,cpu_reset,guest_errors -no-reboot \
		-boot d \
		-serial file:qemu.log \
		-D qemu.log

run_virgl: all
	qemu-system-x86_64 -m 8G -cdrom $(ISO_IMAGE) -serial stdio \
        -drive id=disk,file=$(DISK_IMAGE),if=none,format=raw,readonly=off \
        -device ich9-ahci,id=ahci \
        -device ide-hd,drive=disk,bus=ahci.0 \
        -device virtio-vga-gl \
        -display gtk,gl=on \
		-d int,cpu_reset,guest_errors -no-reboot \
		-boot d \
		-serial file:qemu.log \
		-D qemu.log

run_nograph: all
	qemu-system-x86_64 -m 8G -cdrom $(ISO_IMAGE) \
        -drive id=disk,file=$(DISK_IMAGE),if=none,format=raw,readonly=off \
        -device ich9-ahci,id=ahci \
        -device ide-hd,drive=disk,bus=ahci.0 \
        -nographic \
        -serial mon:stdio \
        -d int,cpu_reset,guest_errors -no-reboot \
        -boot d


dump: kernel.elf
	@echo "[DEBUG] Zrzucanie asemblera..."
	$(HOME)/opt/cross/bin/x86_64-elf-objdump -d -S -M intel kernel.elf > kernel.asm
	@echo "Gotowe! Otwórz plik kernel.asm i szukaj adresu: 10abd7"

clean:
	-rm -rf build iso kernel.elf initrd.tar hello2.txt qemu.log
	-rm -f ams.iso disk.img