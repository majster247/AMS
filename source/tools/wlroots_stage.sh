#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/wlroots-stack"
WLROOTS_DIR="${DEPS_DIR}/wlroots"
XKBCOMMON_DIR="${DEPS_DIR}/libxkbcommon"
LIBSEAT_DIR="${DEPS_DIR}/libseat"

mkdir -p "${DEPS_DIR}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  local branch="${3:-}"
  if [[ ! -d "${dst}/.git" ]]; then
    if [[ -n "${branch}" ]]; then
      git clone --depth 1 --single-branch --branch "${branch}" "${repo_url}" "${dst}"
    else
      git clone --depth 1 --single-branch "${repo_url}" "${dst}"
    fi
  else
    git -C "${dst}" fetch --depth 1 origin
    git -C "${dst}" reset --hard origin/HEAD
  fi
}

echo "[wlroots-stage] Cloning/updating wlroots and dependencies..."

clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}" "0.18"
clone_or_update "https://github.com/xkbcommon/libxkbcommon.git"      "${XKBCOMMON_DIR}"
clone_or_update "https://git.sr.ht/~kennylevinsen/seatd"              "${LIBSEAT_DIR}"

echo "[wlroots-stage] repositories ready:"
echo "  - ${WLROOTS_DIR}"
echo "  - ${XKBCOMMON_DIR}"
echo "  - ${LIBSEAT_DIR}"

echo ""
echo "[wlroots-stage] Build instructions (host cross-compile):"
echo "  wlroots requires: wayland, wayland-protocols, pixman, libdrm, libinput, xkbcommon"
echo "  AMS provides built-in: pixman (include/pixman.h + src/lib/pixman/)"
echo "                         libffi (include/ffi.h + src/lib/libffi/)"
echo "                         libdrm headers (include/drm/)"
echo "                         GBM (include/drm/gbm.h + src/lib/gbm.c)"
echo "                         evdev/input (include/linux/input.h)"
echo ""
echo "  For cross-compilation, configure wlroots meson with:"
echo "    -Dbackends=drm"
echo "    -Drenderers=pixman"
echo "    -Dxwayland=disabled"
echo "    -Dexamples=false"
