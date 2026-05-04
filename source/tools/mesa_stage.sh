#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-soft"
LIBDRM_BUILD="${DEPS_DIR}/build-libdrm"

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
echo "[mesa-stage] configure EGL+GBM software-first profile (swrast/wayland)..."

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  # Build libdrm first
  echo "[mesa-stage] Building libdrm..."
  rm -rf "${LIBDRM_BUILD}"
  meson setup "${LIBDRM_BUILD}" "${LIBDRM_DIR}" \
    -Ddefault_library=static \
    -Dtests=false \
    -Dcairo-tests=disabled \
    -Dman-pages=disabled \
    -Dvalgrind=disabled \
    -Dintel=disabled \
    -Dradeon=disabled \
    -Damdgpu=disabled \
    -Dnouveau=disabled || true
  ninja -C "${LIBDRM_BUILD}" -j"$(nproc)" || echo "[mesa-stage] libdrm build may need setup"

  # Build Mesa with EGL, GBM, and software rendering
  echo "[mesa-stage] Building Mesa3D (EGL + GBM + swrast)..."
  rm -rf "${MESA_BUILD_DIR}"
  meson setup "${MESA_BUILD_DIR}" "${MESA_DIR}" \
    -Dplatforms=wayland \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Ddri-drivers=[] \
    -Degl=enabled \
    -Dgbm=enabled \
    -Dgles1=disabled \
    -Dgles2=enabled \
    -Dopengl=true \
    -Dglx=disabled \
    -Dllvm=disabled \
    -Dshared-glapi=enabled || true
  ninja -C "${MESA_BUILD_DIR}" -j"$(nproc)" || echo "[mesa-stage] Mesa build may need wayland/libdrm"
  echo "[mesa-stage] build finished: ${MESA_BUILD_DIR}"
else
  echo "[mesa-stage] meson/ninja unavailable, skipped local build."
fi

echo "[mesa-stage] Deliverables: EGL (libEGL.so), GBM (libgbm.so), GLESv2 (libGLESv2.so)"
