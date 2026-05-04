#!/usr/bin/env bash
# mesa_stage.sh – Clone and build Mesa3D (EGL + GBM + swrast/llvmpipe) for AMS-OS.
#
# Outputs:
#   external/mesa-stack/build-mesa-soft/  – Meson build tree
#   external/mesa-stack/prefix/           – installed EGL/GBM/GL libraries
#
# The installed prefix is copied to the AMS disk image under
# /programs/wayland/mesa/ by the main Makefile.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-soft"
MESA_PREFIX="${DEPS_DIR}/prefix"

mkdir -p "${DEPS_DIR}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  local branch="${3:-main}"
  if [[ ! -d "${dst}/.git" ]]; then
    git clone --depth 1 --single-branch --branch "${branch}" "${repo_url}" "${dst}" \
      || git clone --depth 1 "${repo_url}" "${dst}"
  else
    git -C "${dst}" fetch --depth 1 origin
    git -C "${dst}" reset --hard origin/HEAD
  fi
}

# ---- libdrm (required by Mesa for GBM) ----
clone_or_update "https://gitlab.freedesktop.org/mesa/drm.git" "${LIBDRM_DIR}" "main"

LIBDRM_BUILD="${DEPS_DIR}/build-libdrm"
LIBDRM_PREFIX="${DEPS_DIR}/prefix"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  if [[ ! -f "${LIBDRM_BUILD}/build.ninja" ]]; then
    meson setup "${LIBDRM_BUILD}" "${LIBDRM_DIR}" \
      --prefix="${LIBDRM_PREFIX}" \
      -Dintel=disabled \
      -Damdgpu=enabled \
      -Dradeon=disabled \
      -Dnouveau=disabled \
      -Dvmwgfx=disabled \
      -Dfreedreno=disabled \
      -Dvc4=disabled \
      -Detnaviv=disabled \
      -Dtests=false
  fi
  ninja -C "${LIBDRM_BUILD}" -j"$(nproc)"
  ninja -C "${LIBDRM_BUILD}" install
  echo "[mesa-stage] libdrm installed → ${LIBDRM_PREFIX}"
fi

# ---- Mesa3D ----
clone_or_update "https://gitlab.freedesktop.org/mesa/mesa.git" "${MESA_DIR}" "main"

echo "[mesa-stage] configuring Mesa (EGL + GBM + swrast + llvmpipe)..."

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  export PKG_CONFIG_PATH="${LIBDRM_PREFIX}/lib/pkgconfig:${LIBDRM_PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"

  if [[ ! -f "${MESA_BUILD_DIR}/build.ninja" ]]; then
    meson setup "${MESA_BUILD_DIR}" "${MESA_DIR}" \
      --prefix="${MESA_PREFIX}" \
      -Dplatforms=wayland \
      -Dgallium-drivers=swrast,zink \
      -Dvulkan-drivers=[] \
      -Ddri-drivers=[] \
      -Dllvm=disabled \
      -Dosmesa=true \
      -Degl=enabled \
      -Dgbm=enabled \
      -Dgles1=enabled \
      -Dgles2=enabled \
      -Dshared-glapi=enabled \
      -Dglx=disabled \
      -Dbuildtype=release
  fi

  ninja -C "${MESA_BUILD_DIR}" -j"$(nproc)"
  ninja -C "${MESA_BUILD_DIR}" install
  echo "[mesa-stage] Mesa installed → ${MESA_PREFIX}"
  echo "[mesa-stage] Available libraries:"
  find "${MESA_PREFIX}/lib" -name "*.so*" 2>/dev/null | head -40 || true
else
  echo "[mesa-stage] meson/ninja not available – skipping build."
  echo "[mesa-stage] Install with: sudo apt-get install meson ninja-build"
  echo "[mesa-stage] Then re-run: make mesa_stage"
fi

echo "[mesa-stage] done."
