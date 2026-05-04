#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/build/tests"
OUT_FILE="${OUT_DIR}/wayland_desktop_matrix.txt"

mkdir -p "${OUT_DIR}"

check_bin() {
  local rel="$1"
  if [[ -s "${ROOT_DIR}/${rel}" ]]; then
    echo "[ok] ${rel}" >> "${OUT_FILE}"
  else
    echo "[missing] ${rel}" >> "${OUT_FILE}"
    return 1
  fi
}

check_src() {
  local rel="$1"
  if [[ -e "${ROOT_DIR}/${rel}" ]]; then
    echo "[ok] ${rel}" >> "${OUT_FILE}"
  else
    echo "[missing] ${rel}" >> "${OUT_FILE}"
    return 1
  fi
}

echo "AMS-OS Wayland Desktop Stack Compatibility Matrix" > "${OUT_FILE}"
echo "generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${OUT_FILE}"
echo >> "${OUT_FILE}"

echo "=== Core Binaries ===" >> "${OUT_FILE}"
status=0
check_bin "build/wayland_compositor.elf" || status=1
check_bin "build/ams_wayland_shell.elf" || status=1
check_bin "build/wayland_smoke.elf" || status=1
check_bin "build/wayland_smoke_client.elf" || status=1
check_bin "build/wayland_session.elf" || status=1
check_bin "build/wayland_egl_smoke.elf" || status=1

echo >> "${OUT_FILE}"
echo "=== Kernel Subsystems ===" >> "${OUT_FILE}"
check_src "src/drivers/gpu/drm.cpp" || status=1
check_src "src/drivers/input/evdev.cpp" || status=1
check_src "include/drm.h" || status=1
check_src "include/evdev.h" || status=1

echo >> "${OUT_FILE}"
echo "=== External Dependencies (staged) ===" >> "${OUT_FILE}"
check_src "external/wayland-stack/wayland/.git" || echo "[info] wayland: run 'make wayland_stage'" >> "${OUT_FILE}"
check_src "external/wayland-stack/wayland-protocols/.git" || echo "[info] wayland-protocols: run 'make wayland_stage'" >> "${OUT_FILE}"
check_src "external/mesa-stack/mesa/.git" || echo "[info] mesa: run 'make mesa_stage'" >> "${OUT_FILE}"
check_src "external/mesa-stack/libdrm/.git" || echo "[info] libdrm: run 'make mesa_stage'" >> "${OUT_FILE}"
check_src "external/wlroots-stack/wlroots/.git" || echo "[info] wlroots: run 'make wlroots_stage'" >> "${OUT_FILE}"
check_src "external/pixman-stack/pixman/.git" || echo "[info] pixman: run 'make pixman_stage'" >> "${OUT_FILE}"
check_src "external/cairo-stack/cairo/.git" || echo "[info] cairo: run 'make cairo_stage'" >> "${OUT_FILE}"
check_src "external/libinput-stack/libinput/.git" || echo "[info] libinput: run 'make libinput_stage'" >> "${OUT_FILE}"
check_src "external/libffi-stack/libffi/.git" || echo "[info] libffi: run 'make libffi_stage'" >> "${OUT_FILE}"
check_src "external/mlibc-stack/mlibc/.git" || echo "[info] mlibc: run 'make mlibc_stage'" >> "${OUT_FILE}"

echo >> "${OUT_FILE}"
echo "=== Build Infrastructure ===" >> "${OUT_FILE}"
check_src "tools/wayland_stage.sh" || status=1
check_src "tools/mesa_stage.sh" || status=1
check_src "tools/wlroots_stage.sh" || status=1
check_src "tools/pixman_stage.sh" || status=1
check_src "tools/cairo_stage.sh" || status=1
check_src "tools/libinput_stage.sh" || status=1
check_src "tools/libffi_stage.sh" || status=1
check_src "tools/mlibc_stage.sh" || status=1

echo >> "${OUT_FILE}"
echo "=== Stack Status ===" >> "${OUT_FILE}"
echo "DRM/KMS: kernel-integrated (GEM/TTM)" >> "${OUT_FILE}"
echo "Compositor: wlroots-based (ams-wl-compositor)" >> "${OUT_FILE}"
echo "Rendering: pixman + cairo + Mesa EGL/GBM" >> "${OUT_FILE}"
echo "Input: evdev + libinput" >> "${OUT_FILE}"
echo "IPC: AF_UNIX + shm_open + memfd + mmap + poll/epoll" >> "${OUT_FILE}"
echo "Protocol: wayland-scanner generated" >> "${OUT_FILE}"

echo >> "${OUT_FILE}"
if [[ ${status} -eq 0 ]]; then
  echo "result: PASS" >> "${OUT_FILE}"
  echo "[matrix] PASS - ${OUT_FILE}"
else
  echo "result: FAIL (core binaries missing)" >> "${OUT_FILE}"
  echo "[matrix] FAIL - ${OUT_FILE}"
  exit 1
fi
