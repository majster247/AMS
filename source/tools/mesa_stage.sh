#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-soft"
MANIFEST="${DEPS_DIR}/manifest.txt"

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

{
  echo "mesa_stage:"
  echo "  libdrm=${LIBDRM_DIR}"
  echo "  mesa=${MESA_DIR}"
} > "${MANIFEST}"

echo "[mesa-stage] repositories ready:"
cat "${MANIFEST}"
echo "[mesa-stage] configure software-first profile (swrast/wayland/egl/gbm)..."

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${MESA_BUILD_DIR}"
  meson setup "${MESA_BUILD_DIR}" "${MESA_DIR}" \
    -Dplatforms=wayland \
    -Degl=enabled \
    -Dgbm=enabled \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Ddri-drivers=[] \
    -Dllvm=disabled
  ninja -C "${MESA_BUILD_DIR}" -j"$(nproc)"
  echo "[mesa-stage] build finished: ${MESA_BUILD_DIR}"
  {
    echo "build=ready"
    echo "wayland=true"
    echo "egl=true"
    echo "gbm=true"
    echo "gallium=swrast"
  } >> "${MANIFEST}"
else
  echo "[mesa-stage] meson/ninja unavailable, skipped local build."
  {
    echo "build=skipped"
    echo "reason=meson-or-ninja-unavailable"
  } >> "${MANIFEST}"
fi
