#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
WAYLAND_DIR="${STAGE_DIR}/wayland"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
WLROOTS_DIR="${STAGE_DIR}/wlroots"
LIBINPUT_DIR="${STAGE_DIR}/libinput"
PIXMAN_DIR="${STAGE_DIR}/pixman"
CAIRO_DIR="${STAGE_DIR}/cairo"
LIBFFI_DIR="${STAGE_DIR}/libffi"
WAYLAND_BUILD_DIR="${STAGE_DIR}/build-wayland"
WLROOTS_BUILD_DIR="${STAGE_DIR}/build-wlroots-ams"
STAMP_FILE="${STAGE_DIR}/wlroots-port-ready.stamp"

mkdir -p "${STAGE_DIR}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  if [[ ! -d "${dst}/.git" ]]; then
    git clone --depth 1 --single-branch "${repo_url}" "${dst}"
  else
    git -C "${dst}" fetch --depth 1 origin
    git -C "${dst}" reset --hard origin/HEAD
  fi
}

clone_or_update "https://github.com/wayland-mirror/wayland.git" "${WAYLAND_DIR}"
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${PROTOCOLS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${LIBINPUT_DIR}"
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${PIXMAN_DIR}"
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${CAIRO_DIR}"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${WAYLAND_BUILD_DIR}"
  meson setup "${WAYLAND_BUILD_DIR}" "${WAYLAND_DIR}" \
    -Ddocumentation=false \
    -Dtests=false
  ninja -C "${WAYLAND_BUILD_DIR}" wayland-scanner
  mkdir -p "${STAGE_DIR}/host-tools/bin"
  cp "${WAYLAND_BUILD_DIR}/wayland-scanner" "${STAGE_DIR}/host-tools/bin/wayland-scanner"
else
  echo "[wayland-stage] meson/ninja unavailable; wayland-scanner build skipped."
fi

cat > "${STAGE_DIR}/ams-wlroots-cross.txt" <<'EOF'
[binaries]
c = 'x86_64-elf-gcc'
cpp = 'x86_64-elf-g++'
ar = 'x86_64-elf-ar'
strip = 'x86_64-elf-strip'
pkgconfig = 'pkg-config'
wayland-scanner = 'wayland-scanner'

[host_machine]
system = 'ams'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
needs_exe_wrapper = true
EOF

cat > "${STAGE_DIR}/ams-wlroots-port.md" <<EOF
# AMS wlroots compositor port

This stage replaces the legacy in-tree compositor with an upstream-source
wlroots route.  The checked out sources are:

- wayland: $(git -C "${WAYLAND_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- wayland-protocols: $(git -C "${PROTOCOLS_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- wlroots: $(git -C "${WLROOTS_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- libinput: $(git -C "${LIBINPUT_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- pixman: $(git -C "${PIXMAN_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- cairo: $(git -C "${CAIRO_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- libffi: $(git -C "${LIBFFI_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)

Kernel/libc prerequisites currently exposed by AMS for wlroots/Wayland work:

- AF_UNIX sockets with sendmsg/recvmsg and SCM_RIGHTS
- poll, ppoll and epoll_create1/epoll_ctl/epoll_wait
- memfd_create plus shm_open/shm_unlink libc compatibility wrappers
- mmap/ftruncate file-backed shared-memory compatibility path
- eventfd

Next porting work should wire AMS DRM/KMS device nodes to libdrm/wlroots
backends, then build wlroots with the generated cross file after Mesa GBM/EGL
is available in the AMS sysroot.
EOF

date -u +%Y-%m-%dT%H:%M:%SZ > "${STAMP_FILE}"

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"
echo "  - ${WLROOTS_DIR}"
echo "  - ${LIBINPUT_DIR}"
echo "  - ${PIXMAN_DIR}"
echo "  - ${CAIRO_DIR}"
echo "  - ${LIBFFI_DIR}"
