#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
DRM_HEADERS_DIR="${DEPS_DIR}/drm-headers"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-soft"
MESA_GBM_BUILD_DIR="${DEPS_DIR}/build-mesa-gbm"

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

mkdir -p "${DRM_HEADERS_DIR}/include/drm"
cp -a "${LIBDRM_DIR}"/include/drm/*.h "${DRM_HEADERS_DIR}/include/drm/" 2>/dev/null || true
cp -a "${LIBDRM_DIR}"/include/libdrm/*.h "${DRM_HEADERS_DIR}/include/" 2>/dev/null || true

echo "[mesa-stage] repositories ready:"
echo "  - ${LIBDRM_DIR}"
echo "  - ${MESA_DIR}"
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

  rm -rf "${MESA_GBM_BUILD_DIR}"
  meson setup "${MESA_GBM_BUILD_DIR}" "${MESA_DIR}" \
    -Dplatforms=wayland \
    -Degl=enabled \
    -Dgbm=enabled \
    -Dopengl=true \
    -Dglx=disabled \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Ddri-drivers=[] \
    -Dllvm=disabled \
    -Dshared-glapi=enabled || {
      echo "[mesa-stage] GBM/EGL profile requires more AMS sysroot dependencies; metadata staged."
      rm -rf "${MESA_GBM_BUILD_DIR}"
    }
else
  echo "[mesa-stage] meson/ninja unavailable, skipped local build."
fi

cat > "${DEPS_DIR}/mesa-port.md" <<EOF
# AMS Mesa3D GBM/EGL source port

Sources:

- libdrm: $(git -C "${LIBDRM_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)
- mesa: $(git -C "${MESA_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)

The AMS userspace ABI now exposes the core Wayland shared-memory primitives
Mesa needs early in the port: AF_UNIX + SCM_RIGHTS, mmap/ftruncate-backed
memfd files, shm_open compatibility wrappers, poll/epoll and eventfd.

The software swrast Wayland profile is attempted first for host validation.
The GBM/EGL profile is configured separately so missing DRM/KMS or sysroot
pieces are visible without blocking source staging.
EOF
