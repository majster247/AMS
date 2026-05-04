#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-soft"

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

echo "[mesa-stage] repositories ready:"
echo "  - ${LIBDRM_DIR}"
echo "  - ${MESA_DIR}"
echo "[mesa-stage] AMS-OS Mesa3D port profile: EGL + GBM (software swrast)"
echo "[mesa-stage] Target: wayland platform with DRM/KMS backend"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${MESA_BUILD_DIR}"
  meson setup "${MESA_BUILD_DIR}" "${MESA_DIR}" \
    -Dplatforms=wayland \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Degl-native-platform=drm \
    -Dgbm=enabled \
    -Degl=enabled \
    -Dglx=disabled \
    -Dllvm=disabled
  ninja -C "${MESA_BUILD_DIR}" -j"$(nproc)"
  echo "[mesa-stage] EGL+GBM build finished: ${MESA_BUILD_DIR}"
else
  echo "[mesa-stage] meson/ninja unavailable, skipped local build."
  echo "[mesa-stage] Headers and cross-compile config staged."
fi
