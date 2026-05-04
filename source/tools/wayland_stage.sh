#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
WAYLAND_DIR="${STAGE_DIR}/wayland"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"

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
echo ""
echo "[wayland-stage] wayland-scanner available at: ${WAYLAND_DIR}/src/scanner.c"
echo "[wayland-stage] Protocol XMLs at: ${PROTOCOLS_DIR}/"

# Build wayland-scanner if meson available
if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  WAYLAND_BUILD="${STAGE_DIR}/build-wayland"
  if [[ ! -d "${WAYLAND_BUILD}" ]]; then
    meson setup "${WAYLAND_BUILD}" "${WAYLAND_DIR}" \
      -Dscanner=true \
      -Dlibraries=false \
      -Dtests=false \
      -Ddocumentation=false
    ninja -C "${WAYLAND_BUILD}" -j"$(nproc)" wayland-scanner 2>/dev/null || true
    echo "[wayland-stage] wayland-scanner build attempted"
  fi
else
  echo "[wayland-stage] meson/ninja unavailable, scanner build skipped."
fi
