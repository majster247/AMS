#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-soft"
PIXMAN_DIR="${DEPS_DIR}/pixman"
CAIRO_DIR="${DEPS_DIR}/cairo"
LIBFFI_DIR="${DEPS_DIR}/libffi"
LIBINPUT_DIR="${DEPS_DIR}/libinput"
WLROOTS_DIR="${DEPS_DIR}/wlroots"

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

clone_or_update "https://gitlab.freedesktop.org/mesa/drm.git" "${LIBDRM_DIR}"
clone_or_update "https://github.com/mesa3d/mesa.git" "${MESA_DIR}"
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${PIXMAN_DIR}"
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${CAIRO_DIR}"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${LIBINPUT_DIR}"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"

echo "[mesa-stage] repositories ready:"
echo "  - ${LIBDRM_DIR}"
echo "  - ${MESA_DIR}"
echo "  - ${PIXMAN_DIR}"
echo "  - ${CAIRO_DIR}"
echo "  - ${LIBFFI_DIR}"
echo "  - ${LIBINPUT_DIR}"
echo "  - ${WLROOTS_DIR}"
echo "[mesa-stage] configure software-first profile (swrast/wayland)..."

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${MESA_BUILD_DIR}"
  meson setup "${MESA_BUILD_DIR}" "${MESA_DIR}" \
    -Dplatforms=wayland \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Ddri-drivers=[] \
    -Dllvm=disabled
  ninja -C "${MESA_BUILD_DIR}" -j"$(nproc)"
  echo "[mesa-stage] build finished: ${MESA_BUILD_DIR}"
else
  echo "[mesa-stage] meson/ninja unavailable, skipped local build."
fi
