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
MLIBC_DIR="${STAGE_DIR}/mlibc"

mkdir -p "${STAGE_DIR}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  local mirror_url="${3:-}"
  if [[ ! -d "${dst}/.git" ]]; then
    git clone --depth 1 --single-branch "${repo_url}" "${dst}" || {
      if [[ -n "${mirror_url}" ]]; then
        rm -rf "${dst}"
        git clone --depth 1 --single-branch "${mirror_url}" "${dst}"
      else
        return 1
      fi
    }
  else
    git -C "${dst}" fetch --depth 1 origin || {
      if [[ -n "${mirror_url}" ]]; then
        git -C "${dst}" remote set-url origin "${mirror_url}"
        git -C "${dst}" fetch --depth 1 origin
      else
        return 1
      fi
    }
    git -C "${dst}" reset --hard origin/HEAD
  fi
}

clone_or_update "https://github.com/wayland-mirror/wayland.git" "${WAYLAND_DIR}"
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${PROTOCOLS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}" "https://github.com/swaywm/wlroots.git"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${LIBINPUT_DIR}"
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${PIXMAN_DIR}" "https://github.com/freedesktop/pixman.git"
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${CAIRO_DIR}" "https://github.com/freedesktop/cairo.git"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"
clone_or_update "https://github.com/managarm/mlibc.git" "${MLIBC_DIR}"

# wlroots vendors subprojects for selected fallback dependencies. Keep them
# available without assuming the host has a complete freedesktop stack.
git -C "${WLROOTS_DIR}" submodule update --init --recursive

if command -v wayland-scanner >/dev/null 2>&1; then
  SCANNER="$(command -v wayland-scanner)"
else
  SCANNER="${WAYLAND_DIR}/build/src/wayland-scanner"
fi

mkdir -p "${STAGE_DIR}/generated"
if [[ -x "${SCANNER}" ]]; then
  "${SCANNER}" server-header \
    "${WAYLAND_DIR}/protocol/wayland.xml" \
    "${STAGE_DIR}/generated/wayland-server-protocol.h"
  "${SCANNER}" private-code \
    "${WAYLAND_DIR}/protocol/wayland.xml" \
    "${STAGE_DIR}/generated/wayland-protocol.c"
  echo "[wayland-stage] generated core protocol with ${SCANNER}"
else
  echo "[wayland-stage] wayland-scanner unavailable; run a host Wayland build before compiling wlroots."
fi

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"
echo "  - ${WLROOTS_DIR}"
echo "  - ${LIBINPUT_DIR}"
echo "  - ${PIXMAN_DIR}"
echo "  - ${CAIRO_DIR}"
echo "  - ${LIBFFI_DIR}"
echo "  - ${MLIBC_DIR}"
