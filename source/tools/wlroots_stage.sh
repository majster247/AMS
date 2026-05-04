#!/usr/bin/env bash
# wlroots_stage.sh – Clone and build wlroots + all its dependencies for AMS-OS.
#
# Dependencies built here:
#   - wayland        (libwayland-server, libwayland-client)
#   - wayland-protocols
#   - libinput       (event input abstraction)
#   - pixman         (CPU compositing)
#   - cairo          (desktop chrome / title bars)
#   - xkbcommon      (keyboard layout)
#   - wlroots        (Wayland compositor toolkit)
#
# After a successful build, wayland-scanner is used to generate C bindings
# for the protocols required by the AMS compositor:
#   - xdg-shell
#   - xdg-output-unstable-v1
#   - wlr-layer-shell-unstable-v1
#
# Outputs:
#   external/wlroots-stack/prefix/   – installed headers + libraries
#   external/wlroots-stack/protocols/ – generated protocol glue code

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wlroots-stack"
PREFIX="${STAGE_DIR}/prefix"
PROTOCOLS_OUT="${STAGE_DIR}/protocols"

MESA_PREFIX="${ROOT_DIR}/external/mesa-stack/prefix"
WL_STACK_DIR="${ROOT_DIR}/external/wayland-stack"

mkdir -p "${STAGE_DIR}" "${PREFIX}" "${PROTOCOLS_OUT}"

clone_or_update() {
  local url="$1"
  local dst="$2"
  local branch="${3:-main}"
  if [[ ! -d "${dst}/.git" ]]; then
    git clone --depth 1 --single-branch --branch "${branch}" "${url}" "${dst}" \
      || git clone --depth 1 "${url}" "${dst}"
  else
    git -C "${dst}" fetch --depth 1 origin
    git -C "${dst}" reset --hard origin/HEAD
  fi
}

need_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[wlroots-stage] ERROR: '$1' not found. Install it and retry."
    exit 1
  fi
}

need_tool meson
need_tool ninja
need_tool pkg-config

export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PREFIX}/lib64/pkgconfig:${MESA_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export PATH="${PREFIX}/bin:${PATH}"

meson_build() {
  local name="$1"
  local src="$2"
  local build="${STAGE_DIR}/build-${name}"
  shift 2
  if [[ ! -f "${build}/build.ninja" ]]; then
    meson setup "${build}" "${src}" --prefix="${PREFIX}" "$@"
  fi
  ninja -C "${build}" -j"$(nproc)"
  ninja -C "${build}" install
  echo "[wlroots-stage] ${name} installed."
}

# ---- wayland ----
clone_or_update "https://gitlab.freedesktop.org/wayland/wayland.git" \
                "${WL_STACK_DIR}/wayland" "main"
meson_build wayland "${WL_STACK_DIR}/wayland" \
  -Dtests=false -Ddocumentation=false

# ---- wayland-protocols ----
clone_or_update "https://gitlab.freedesktop.org/wayland/wayland-protocols.git" \
                "${WL_STACK_DIR}/wayland-protocols" "main"
meson_build wayland-protocols "${WL_STACK_DIR}/wayland-protocols" \
  -Dtests=false

# ---- xkbcommon ----
clone_or_update "https://github.com/xkbcommon/libxkbcommon.git" \
                "${STAGE_DIR}/xkbcommon" "master"
meson_build xkbcommon "${STAGE_DIR}/xkbcommon" \
  -Denable-docs=false \
  -Denable-wayland=true \
  -Denable-x11=false

# ---- pixman ----
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" \
                "${STAGE_DIR}/pixman" "master"
meson_build pixman "${STAGE_DIR}/pixman" \
  -Dtests=disabled \
  -Ddemos=disabled

# ---- libinput ----
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" \
                "${STAGE_DIR}/libinput" "main"
meson_build libinput "${STAGE_DIR}/libinput" \
  -Dtests=false \
  -Ddocumentation=false \
  -Ddebug-gui=false \
  -Dlibwacom=false \
  -Dudev-dir="${PREFIX}/lib/udev"

# ---- cairo ----
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" \
                "${STAGE_DIR}/cairo" "master"
meson_build cairo "${STAGE_DIR}/cairo" \
  -Dtee=enabled \
  -Dfontconfig=disabled \
  -Dfreetype=disabled \
  -Dxlib=disabled \
  -Dxcb=disabled \
  -Dtests=disabled

# ---- wlroots ----
WLROOTS_DIR="${STAGE_DIR}/wlroots"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" \
                "${WLROOTS_DIR}" "master"
meson_build wlroots "${WLROOTS_DIR}" \
  -Dbackends=drm,libinput \
  -Drenderers=gles2,pixman \
  -Dallocators=gbm \
  -Dxwayland=disabled \
  -Dexamples=false

echo "[wlroots-stage] wlroots and all dependencies installed → ${PREFIX}"

# ---- wayland-scanner: generate protocol bindings ----
SCANNER="${PREFIX}/bin/wayland-scanner"
if [[ ! -x "${SCANNER}" ]]; then
  SCANNER="$(command -v wayland-scanner 2>/dev/null || echo '')"
fi

if [[ -z "${SCANNER}" ]]; then
  echo "[wlroots-stage] WARNING: wayland-scanner not found, skipping protocol generation."
else
  echo "[wlroots-stage] Generating protocol bindings with ${SCANNER}..."

  PROTOCOLS_DATA="${PREFIX}/share/wayland-protocols"
  WLR_PROTOCOLS="${WLROOTS_DIR}/protocol"

  generate_protocol() {
    local xml="$1"
    local base
    base="$(basename "${xml}" .xml)"
    "${SCANNER}" private-code  "${xml}" "${PROTOCOLS_OUT}/${base}-protocol.c"
    "${SCANNER}" client-header "${xml}" "${PROTOCOLS_OUT}/${base}-client-protocol.h"
    "${SCANNER}" server-header "${xml}" "${PROTOCOLS_OUT}/${base}-server-protocol.h"
    echo "  generated: ${base}"
  }

  # xdg-shell
  XDG_SHELL_XML="${PROTOCOLS_DATA}/stable/xdg-shell/xdg-shell.xml"
  [[ -f "${XDG_SHELL_XML}" ]] && generate_protocol "${XDG_SHELL_XML}"

  # xdg-output-unstable-v1
  XDG_OUTPUT_XML="${PROTOCOLS_DATA}/unstable/xdg-output/xdg-output-unstable-v1.xml"
  [[ -f "${XDG_OUTPUT_XML}" ]] && generate_protocol "${XDG_OUTPUT_XML}"

  # wlr-layer-shell-unstable-v1
  LAYER_SHELL_XML="${WLR_PROTOCOLS}/wlr-layer-shell-unstable-v1.xml"
  [[ -f "${LAYER_SHELL_XML}" ]] && generate_protocol "${LAYER_SHELL_XML}"

  # wlr-output-management-unstable-v1
  OUTPUT_MGMT_XML="${WLR_PROTOCOLS}/wlr-output-management-unstable-v1.xml"
  [[ -f "${OUTPUT_MGMT_XML}" ]] && generate_protocol "${OUTPUT_MGMT_XML}"

  echo "[wlroots-stage] Protocol bindings written to ${PROTOCOLS_OUT}/"
fi

echo "[wlroots-stage] done."
