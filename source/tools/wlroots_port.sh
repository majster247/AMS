#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STACK_DIR="${ROOT_DIR}/external/graphics-stack"
WLROOTS_DIR="${STACK_DIR}/wlroots"
BUILD_DIR="${STACK_DIR}/build-wlroots"

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

mkdir -p "${STACK_DIR}"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"

echo "[wlroots-port] repository ready:"
echo "  - ${WLROOTS_DIR}"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${BUILD_DIR}"
  meson setup "${BUILD_DIR}" "${WLROOTS_DIR}" \
    -Dbackends=drm,libinput \
    -Drenderers=gles2,pixman \
    -Dxwayland=disabled \
    -Dexamples=false
  ninja -C "${BUILD_DIR}" -j"$(nproc)"
  echo "[wlroots-port] build finished: ${BUILD_DIR}"
else
  echo "[wlroots-port] meson/ninja unavailable, skipped local build."
fi
