#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/graphics-stack"

WAYLAND_DIR="${STAGE_DIR}/wayland"
WAYLAND_PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
WLROOTS_DIR="${STAGE_DIR}/wlroots"
LIBINPUT_DIR="${STAGE_DIR}/libinput"
LIBFFI_DIR="${STAGE_DIR}/libffi"
PIXMAN_DIR="${STAGE_DIR}/pixman"
CAIRO_DIR="${STAGE_DIR}/cairo"
LIBDRM_DIR="${STAGE_DIR}/libdrm"
MESA_DIR="${STAGE_DIR}/mesa"

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
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${WAYLAND_PROTOCOLS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${LIBINPUT_DIR}"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${PIXMAN_DIR}"
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${CAIRO_DIR}"
clone_or_update "https://gitlab.freedesktop.org/mesa/drm.git" "${LIBDRM_DIR}"
clone_or_update "https://github.com/mesa3d/mesa.git" "${MESA_DIR}"

echo "[graphics-stage] repositories ready:"
for dir in \
  "${WAYLAND_DIR}" \
  "${WAYLAND_PROTOCOLS_DIR}" \
  "${WLROOTS_DIR}" \
  "${LIBINPUT_DIR}" \
  "${LIBFFI_DIR}" \
  "${PIXMAN_DIR}" \
  "${CAIRO_DIR}" \
  "${LIBDRM_DIR}" \
  "${MESA_DIR}"; do
  printf '  - %s\n' "${dir}"
done

if [[ -x "${WAYLAND_DIR}/build-aux/wayland-scanner.mk" || -f "${WAYLAND_DIR}/src/wayland-scanner.c" ]]; then
  echo "[graphics-stage] wayland-scanner sources available in ${WAYLAND_DIR}"
fi
