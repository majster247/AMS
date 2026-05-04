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
MLIBC_DIR="${STAGE_DIR}/mlibc"
LIBFFI_DIR="${STAGE_DIR}/libffi"
MANIFEST_FILE="${STAGE_DIR}/manifest.txt"

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
clone_or_update "https://github.com/managarm/mlibc.git" "${MLIBC_DIR}"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"
echo "  - ${WLROOTS_DIR}"
echo "  - ${LIBINPUT_DIR}"
echo "  - ${PIXMAN_DIR}"
echo "  - ${CAIRO_DIR}"
echo "  - ${MLIBC_DIR}"
echo "  - ${LIBFFI_DIR}"

{
  echo "wayland=$(git -C "${WAYLAND_DIR}" rev-parse --short HEAD)"
  echo "wayland-protocols=$(git -C "${PROTOCOLS_DIR}" rev-parse --short HEAD)"
  echo "wlroots=$(git -C "${WLROOTS_DIR}" rev-parse --short HEAD)"
  echo "libinput=$(git -C "${LIBINPUT_DIR}" rev-parse --short HEAD)"
  echo "pixman=$(git -C "${PIXMAN_DIR}" rev-parse --short HEAD)"
  echo "cairo=$(git -C "${CAIRO_DIR}" rev-parse --short HEAD)"
  echo "mlibc=$(git -C "${MLIBC_DIR}" rev-parse --short HEAD)"
  echo "libffi=$(git -C "${LIBFFI_DIR}" rev-parse --short HEAD)"
  if command -v wayland-scanner >/dev/null 2>&1; then
    echo "wayland-scanner=$(command -v wayland-scanner)"
  else
    echo "wayland-scanner=missing"
  fi
} > "${MANIFEST_FILE}"

echo "[wayland-stage] manifest written to ${MANIFEST_FILE}"
