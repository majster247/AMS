#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mesa-stack"
MESA_DIR="${DEPS_DIR}/mesa"
LIBDRM_DIR="${DEPS_DIR}/libdrm"
MESA_BUILD_DIR="${DEPS_DIR}/build-mesa-ams"

mkdir -p "${DEPS_DIR}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  if [[ ! -d "${dst}/.git" ]]; then
    for attempt in 1 2 3 4; do
      git clone --depth 1 --single-branch "${repo_url}" "${dst}" && break
      rm -rf "${dst}"
      if [[ "${attempt}" == "4" ]]; then return 1; fi
      sleep $((attempt * 4))
    done
  else
    for attempt in 1 2 3 4; do
      git -C "${dst}" fetch --depth 1 origin && git -C "${dst}" reset --hard origin/HEAD && break
      if [[ "${attempt}" == "4" ]]; then return 1; fi
      sleep $((attempt * 4))
    done
  fi
}

clone_or_update "https://gitlab.freedesktop.org/mesa/drm.git" "${LIBDRM_DIR}"
clone_or_update "https://github.com/mesa3d/mesa.git" "${MESA_DIR}"

echo "[mesa-stage] repositories ready:"
echo "  - ${LIBDRM_DIR}"
echo "  - ${MESA_DIR}"
echo "[mesa-stage] configure AMS target profile (EGL/GBM/Gallium swrast, no Vulkan)..."

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${MESA_BUILD_DIR}"
  cross_file="${ROOT_DIR}/tools/cross/ams-x86_64.meson"
  cross_args=()
  if [[ -f "${cross_file}" ]]; then
    cross_args=(--cross-file "${cross_file}")
  else
    echo "[mesa-stage] AMS Meson cross file missing; using host configure as a dependency smoke test."
  fi
  meson setup "${MESA_BUILD_DIR}" "${MESA_DIR}" \
    "${cross_args[@]}" \
    -Dplatforms=wayland \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Ddri-drivers=[] \
    -Dllvm=disabled \
    -Degl=enabled \
    -Dgbm=enabled \
    -Dgles1=disabled \
    -Dgles2=enabled \
    -Dglx=disabled \
    -Dshared-glapi=enabled \
    -Dosmesa=false \
    -Dglvnd=disabled
  ninja -C "${MESA_BUILD_DIR}" -j"$(nproc)"
  echo "[mesa-stage] build finished: ${MESA_BUILD_DIR}"
else
  echo "[mesa-stage] meson/ninja unavailable, skipped local build."
fi
