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

clone_with_fallback() {
  local primary_url="$1"
  local fallback_url="$2"
  local dst="$3"
  if clone_or_update "${primary_url}" "${dst}"; then
    return 0
  fi
  echo "[wayland-stage] primary failed for ${dst}, trying fallback..."
  rm -rf "${dst}"
  clone_or_update "${fallback_url}" "${dst}"
}

clone_optional_with_fallback() {
  local primary_url="$1"
  local fallback_url="$2"
  local dst="$3"
  if clone_or_update "${primary_url}" "${dst}"; then
    return 0
  fi
  echo "[wayland-stage] optional primary failed for ${dst}, trying fallback..."
  rm -rf "${dst}"
  if clone_or_update "${fallback_url}" "${dst}"; then
    return 0
  fi
  echo "[wayland-stage] optional repository unavailable: ${dst}"
  return 0
}

clone_or_update "https://github.com/wayland-mirror/wayland.git" "${WAYLAND_DIR}"
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${PROTOCOLS_DIR}"
clone_or_update "https://github.com/swaywm/wlroots.git" "${WLROOTS_DIR}"
clone_optional_with_fallback "https://gitlab.freedesktop.org/libinput/libinput.git" "https://github.com/freedesktop-unofficial-mirror/libinput.git" "${LIBINPUT_DIR}"
clone_optional_with_fallback "https://gitlab.freedesktop.org/pixman/pixman.git" "https://github.com/freedesktop-unofficial-mirror/pixman.git" "${PIXMAN_DIR}"
clone_optional_with_fallback "https://gitlab.freedesktop.org/cairo/cairo.git" "https://github.com/freedesktop-unofficial-mirror/cairo.git" "${CAIRO_DIR}"

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"
echo "  - ${WLROOTS_DIR}"
echo "  - ${LIBINPUT_DIR}"
echo "  - ${PIXMAN_DIR}"
echo "  - ${CAIRO_DIR}"

if command -v wayland-scanner >/dev/null 2>&1; then
  mkdir -p "${STAGE_DIR}/generated"
  wayland-scanner client-header \
    "${PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" \
    "${STAGE_DIR}/generated/xdg-shell-client-protocol.h"
  wayland-scanner private-code \
    "${PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" \
    "${STAGE_DIR}/generated/xdg-shell-protocol.c"
  echo "[wayland-stage] wayland-scanner generated xdg-shell protocol stubs."
else
  echo "[wayland-stage] wayland-scanner unavailable; skipped protocol generation."
fi
