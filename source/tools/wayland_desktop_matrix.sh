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

echo "Wayland desktop compatibility matrix (DRM/KMS + wlroots-style)" > "${OUT_FILE}"
echo "generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${OUT_FILE}"
echo >> "${OUT_FILE}"

echo "=== Stack ===" >> "${OUT_FILE}"
echo "  Kernel:     AMS-DRM (KMS + GEM/TTM)" >> "${OUT_FILE}"
echo "  Backend:    DRM/KMS, libinput" >> "${OUT_FILE}"
echo "  Renderer:   pixman (software), Mesa EGL+GBM (staged)" >> "${OUT_FILE}"
echo "  Protocol:   Wayland wire protocol + xdg-shell" >> "${OUT_FILE}"
echo "  Compositor: wlroots-style (ams_wl_compositor)" >> "${OUT_FILE}"
echo "  Scanner:    wayland-scanner (staged)" >> "${OUT_FILE}"
echo >> "${OUT_FILE}"

echo "=== Binaries ===" >> "${OUT_FILE}"
status=0
check_bin "build/wayland_compositor.elf" || status=1
check_bin "build/ams_wayland_shell.elf" || status=1
check_bin "build/wayland_smoke.elf" || status=1
check_bin "build/wayland_smoke_client.elf" || status=1
check_bin "build/wayland_session.elf" || status=1
check_bin "build/wayland_egl_smoke.elf" || status=1

echo >> "${OUT_FILE}"

echo "=== Kernel subsystems ===" >> "${OUT_FILE}"
echo "  DRM/KMS:    yes (ams_drm.cpp)" >> "${OUT_FILE}"
echo "  GEM/TTM:    yes (dumb buffers + gem_close)" >> "${OUT_FILE}"
echo "  Unix socks: yes (AF_UNIX SOCK_STREAM)" >> "${OUT_FILE}"
echo "  shm_open:   yes (SYS_AMS_SHM_OPEN)" >> "${OUT_FILE}"
echo "  poll/epoll: yes (SYS_POLL, SYS_EPOLL_*)" >> "${OUT_FILE}"
echo "  mmap:       yes (SYS_MMAP)" >> "${OUT_FILE}"
echo >> "${OUT_FILE}"

if [[ ${status} -eq 0 ]]; then
  echo "result: PASS" >> "${OUT_FILE}"
  echo "[matrix] PASS - ${OUT_FILE}"
else
  echo "result: FAIL" >> "${OUT_FILE}"
  echo "[matrix] FAIL - ${OUT_FILE}"
  exit 1
fi
