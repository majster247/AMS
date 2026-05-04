#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
WAYLAND_DIR="${STAGE_DIR}/wayland"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
SCANNER_BUILD="${STAGE_DIR}/build-scanner"

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

clone_or_update "https://github.com/wayland-mirror/wayland.git" "${WAYLAND_DIR}"
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${PROTOCOLS_DIR}"

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"

# Build wayland-scanner for the host (needed for protocol code generation)
if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  echo "[wayland-stage] Building wayland-scanner for host..."
  rm -rf "${SCANNER_BUILD}"
  meson setup "${SCANNER_BUILD}" "${WAYLAND_DIR}" \
    -Dscanner=true \
    -Dlibraries=false \
    -Dtests=false \
    -Ddocumentation=false || true
  ninja -C "${SCANNER_BUILD}" -j"$(nproc)" wayland-scanner || \
    echo "[wayland-stage] scanner build may need expat/libxml2"

  if [[ -f "${SCANNER_BUILD}/wayland-scanner" ]]; then
    echo "[wayland-stage] wayland-scanner ready at ${SCANNER_BUILD}/wayland-scanner"
    
    # Generate protocol C sources
    PROTO_OUT="${STAGE_DIR}/generated"
    mkdir -p "${PROTO_OUT}"
    SCANNER="${SCANNER_BUILD}/wayland-scanner"

    # Core Wayland protocol
    if [[ -f "${WAYLAND_DIR}/protocol/wayland.xml" ]]; then
      "${SCANNER}" server-header "${WAYLAND_DIR}/protocol/wayland.xml" \
        "${PROTO_OUT}/wayland-server-protocol.h" 2>/dev/null || true
      "${SCANNER}" client-header "${WAYLAND_DIR}/protocol/wayland.xml" \
        "${PROTO_OUT}/wayland-client-protocol.h" 2>/dev/null || true
      "${SCANNER}" private-code "${WAYLAND_DIR}/protocol/wayland.xml" \
        "${PROTO_OUT}/wayland-protocol.c" 2>/dev/null || true
      echo "[wayland-stage] Generated core protocol sources"
    fi

    # XDG shell
    XDG_SHELL="${PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    if [[ -f "${XDG_SHELL}" ]]; then
      "${SCANNER}" server-header "${XDG_SHELL}" \
        "${PROTO_OUT}/xdg-shell-server-protocol.h" 2>/dev/null || true
      "${SCANNER}" client-header "${XDG_SHELL}" \
        "${PROTO_OUT}/xdg-shell-client-protocol.h" 2>/dev/null || true
      "${SCANNER}" private-code "${XDG_SHELL}" \
        "${PROTO_OUT}/xdg-shell-protocol.c" 2>/dev/null || true
      echo "[wayland-stage] Generated xdg-shell protocol sources"
    fi

    # Linux DMA-BUF
    DMABUF="${PROTOCOLS_DIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml"
    if [[ -f "${DMABUF}" ]]; then
      "${SCANNER}" server-header "${DMABUF}" \
        "${PROTO_OUT}/linux-dmabuf-unstable-v1-server-protocol.h" 2>/dev/null || true
      "${SCANNER}" private-code "${DMABUF}" \
        "${PROTO_OUT}/linux-dmabuf-unstable-v1-protocol.c" 2>/dev/null || true
      echo "[wayland-stage] Generated linux-dmabuf protocol sources"
    fi
  fi
else
  echo "[wayland-stage] meson/ninja unavailable, skipped scanner build."
fi
