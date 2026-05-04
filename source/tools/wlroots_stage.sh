#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/wlroots-stack"
WLROOTS_DIR="${DEPS_DIR}/wlroots"
WAYLAND_DIR="${ROOT_DIR}/external/wayland-stack/wayland"
PROTOCOLS_DIR="${ROOT_DIR}/external/wayland-stack/wayland-protocols"

mkdir -p "${DEPS_DIR}"

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

clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"

echo "[wlroots-stage] repository ready: ${WLROOTS_DIR}"
echo "[wlroots-stage] Dependencies: wayland, wayland-protocols, pixman, libdrm, libinput, mesa"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${DEPS_DIR}/build"
  meson setup "${DEPS_DIR}/build" "${WLROOTS_DIR}" \
    -Ddefault_library=static \
    -Dxwayland=disabled \
    -Dbackends=drm,libinput \
    -Drenderers=gles2,pixman \
    -Dexamples=false || true
  ninja -C "${DEPS_DIR}/build" -j"$(nproc)" || echo "[wlroots-stage] build needs dependency setup"
  echo "[wlroots-stage] build attempted"
else
  echo "[wlroots-stage] meson/ninja unavailable, source-only stage."
fi
