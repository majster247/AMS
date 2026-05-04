#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wlroots-stack"
WLROOTS_DIR="${STAGE_DIR}/wlroots"
LIBINPUT_DIR="${STAGE_DIR}/libinput"
PIXMAN_DIR="${STAGE_DIR}/pixman"
CAIRO_DIR="${STAGE_DIR}/cairo"
LIBFFI_DIR="${STAGE_DIR}/libffi"

mkdir -p "${STAGE_DIR}"

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

echo "[wlroots-stage] Cloning/updating upstream sources..."

clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${LIBINPUT_DIR}"
clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${PIXMAN_DIR}"
clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${CAIRO_DIR}"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"

echo "[wlroots-stage] repositories ready:"
echo "  - wlroots:  ${WLROOTS_DIR}"
echo "  - libinput: ${LIBINPUT_DIR}"
echo "  - pixman:   ${PIXMAN_DIR}"
echo "  - cairo:    ${CAIRO_DIR}"
echo "  - libffi:   ${LIBFFI_DIR}"
echo ""
echo "[wlroots-stage] AMS-OS wlroots port configuration:"
echo "  backend:    DRM/KMS (ams-drm) + libinput"
echo "  renderer:   pixman (software) + EGL/GBM (Mesa swrast)"
echo "  allocator:  GBM (dumb buffer GEM)"
echo "  scanner:    wayland-scanner (from wayland-stack)"
echo ""

# Build pixman if meson available (dependency for wlroots)
if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  PIXMAN_BUILD="${STAGE_DIR}/build-pixman"
  if [[ ! -d "${PIXMAN_BUILD}" ]]; then
    meson setup "${PIXMAN_BUILD}" "${PIXMAN_DIR}" \
      -Ddefault_library=static \
      -Dgtk=disabled \
      -Dlibpng=disabled \
      -Dopenmp=disabled
    ninja -C "${PIXMAN_BUILD}" -j"$(nproc)"
    echo "[wlroots-stage] pixman built: ${PIXMAN_BUILD}"
  fi

  # Build libffi
  LIBFFI_BUILD="${STAGE_DIR}/build-libffi"
  if [[ ! -d "${LIBFFI_BUILD}" ]]; then
    cd "${LIBFFI_DIR}"
    if [[ ! -f configure ]]; then
      autoreconf -fi 2>/dev/null || true
    fi
    if [[ -f configure ]]; then
      mkdir -p "${LIBFFI_BUILD}"
      cd "${LIBFFI_BUILD}"
      "${LIBFFI_DIR}/configure" --prefix="${LIBFFI_BUILD}/install" --disable-shared
      make -j"$(nproc)"
      make install
      echo "[wlroots-stage] libffi built: ${LIBFFI_BUILD}"
    fi
    cd "${ROOT_DIR}"
  fi
else
  echo "[wlroots-stage] meson/ninja unavailable, skipped host builds."
  echo "[wlroots-stage] Source trees and AMS headers staged for cross-compilation."
fi
