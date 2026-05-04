#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STACK_DIR="${ROOT_DIR}/external/graphics-stack"
SRC_DIR="${STACK_DIR}/src"
BUILD_DIR="${STACK_DIR}/build"
PREFIX_DIR="${STACK_DIR}/prefix"
MANIFEST="${STACK_DIR}/manifest.txt"

mkdir -p "${SRC_DIR}" "${BUILD_DIR}" "${PREFIX_DIR}"

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

clone_or_update "https://github.com/wayland-mirror/wayland.git" "${SRC_DIR}/wayland"
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${SRC_DIR}/wayland-protocols"
clone_or_update "https://gitlab.freedesktop.org/mesa/drm.git" "${SRC_DIR}/libdrm"
clone_or_update "https://gitlab.freedesktop.org/mesa/mesa.git" "${SRC_DIR}/mesa"
clone_or_update "https://github.com/Distrotech/libffi.git" "${SRC_DIR}/libffi"
clone_or_update "https://github.com/managarm/mlibc.git" "${SRC_DIR}/mlibc"
clone_or_update "https://gitlab.freedesktop.org/wayland/weston.git" "${SRC_DIR}/weston"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${SRC_DIR}/libinput"
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${SRC_DIR}/pixman"
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${SRC_DIR}/cairo"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${SRC_DIR}/wlroots"

if command -v wayland-scanner >/dev/null 2>&1; then
  echo "[graphics-stack-stage] wayland-scanner detected."
else
  echo "[graphics-stack-stage] wayland-scanner missing on host; protocol codegen disabled for now."
fi

{
  echo "graphics-stack stage manifest"
  echo "generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  for proj in wayland wayland-protocols libdrm mesa libffi mlibc weston libinput pixman cairo wlroots; do
    if [[ -d "${SRC_DIR}/${proj}/.git" ]]; then
      echo "${proj} $(git -C "${SRC_DIR}/${proj}" rev-parse --short HEAD)"
    fi
  done
} > "${MANIFEST}"

echo "[graphics-stack-stage] repositories ready under ${SRC_DIR}"
echo "[graphics-stack-stage] manifest: ${MANIFEST}"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  WAYLAND_BUILD="${BUILD_DIR}/wayland"
  MESA_BUILD="${BUILD_DIR}/mesa-egl-gbm"
  WLROOTS_BUILD="${BUILD_DIR}/wlroots"

  rm -rf "${WAYLAND_BUILD}" "${MESA_BUILD}" "${WLROOTS_BUILD}"

  meson setup "${WAYLAND_BUILD}" "${SRC_DIR}/wayland" \
    --prefix="${PREFIX_DIR}" \
    -Ddocumentation=false \
    -Dtests=false
  ninja -C "${WAYLAND_BUILD}" -j"$(nproc)"

  meson setup "${MESA_BUILD}" "${SRC_DIR}/mesa" \
    --prefix="${PREFIX_DIR}" \
    -Dplatforms=wayland \
    -Dgallium-drivers=swrast \
    -Dvulkan-drivers=[] \
    -Ddri-drivers=[] \
    -Degl=enabled \
    -Dgbm=enabled \
    -Dllvm=disabled \
    -Dshared-glapi=enabled
  ninja -C "${MESA_BUILD}" -j"$(nproc)"

  if command -v pkg-config >/dev/null 2>&1; then
    export PKG_CONFIG_PATH="${PREFIX_DIR}/lib/pkgconfig:${PREFIX_DIR}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
  fi

  meson setup "${WLROOTS_BUILD}" "${SRC_DIR}/wlroots" \
    --prefix="${PREFIX_DIR}" \
    -Dexamples=false \
    -Dxwayland=disabled \
    -Drenderers=gles2 \
    -Dbackends=drm,libinput \
    -Dsession=disabled
  ninja -C "${WLROOTS_BUILD}" -j"$(nproc)"

  echo "[graphics-stack-stage] meson/ninja builds completed."
else
  echo "[graphics-stack-stage] meson/ninja missing, only source checkout completed."
fi

WLR_BIN_DIR="${ROOT_DIR}/programs/wayland/wlroots/bin"
mkdir -p "${WLR_BIN_DIR}"
if [[ ! -x "${WLR_BIN_DIR}/ams-wlroots-compositor" ]]; then
  cat > "${WLR_BIN_DIR}/ams-wlroots-compositor" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
echo "ams-wlroots-compositor: staged placeholder (wlroots runtime pending kernel DRM/KMS+GEM/TTM)"
exit 0
EOF
  chmod +x "${WLR_BIN_DIR}/ams-wlroots-compositor"
fi
