#!/usr/bin/env bash
#
# wayland_scanner_gen.sh - run wayland-scanner against the staged
# wayland-protocols XML descriptors and produce both private-code and
# client-header artifacts under build/wayland-protocols/.
#
# The compositor and smoke clients pick these up via -I to gain
# strongly-typed wrappers for xdg-shell, wlr-layer-shell, etc.
#
# Without wayland-scanner installed, the script is a no-op (the
# compositor source guards xdg-shell usage with __has_include).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack/wayland-protocols"
OUT_DIR="${ROOT_DIR}/build/wayland-protocols"

mkdir -p "${OUT_DIR}"

if ! command -v wayland-scanner >/dev/null 2>&1; then
  echo "[wayland-scanner] not installed; skipping protocol generation."
  echo "wayland-scanner not present at $(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    > "${OUT_DIR}/SKIPPED.txt"
  exit 0
fi

if [[ ! -d "${STAGE_DIR}" ]]; then
  echo "[wayland-scanner] wayland-protocols not staged; skipping."
  exit 0
fi

# A curated subset that the compositor + smoke clients need.
PROTO_FILES=(
  "stable/xdg-shell/xdg-shell.xml"
  "staging/cursor-shape/cursor-shape-v1.xml"
  "staging/single-pixel-buffer/single-pixel-buffer-v1.xml"
  "unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml"
)

for rel in "${PROTO_FILES[@]}"; do
  src="${STAGE_DIR}/${rel}"
  if [[ ! -f "${src}" ]]; then
    echo "[wayland-scanner] missing ${rel}; skipping"
    continue
  fi
  base="$(basename "${rel}" .xml)"
  wayland-scanner private-code  "${src}" "${OUT_DIR}/${base}-protocol.c"
  wayland-scanner client-header "${src}" "${OUT_DIR}/${base}-client-protocol.h"
  wayland-scanner server-header "${src}" "${OUT_DIR}/${base}-server-protocol.h"
  echo "[wayland-scanner] generated ${base}"
done
