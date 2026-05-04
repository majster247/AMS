#!/usr/bin/env bash
# wayland_scan.sh — regenerate Wayland protocol C headers from XML sources.
#
# Run this on a Linux host that has wayland-scanner installed.
# The generated headers are committed under include/wayland-generated/ so the
# AMS freestanding toolchain does not need to run wayland-scanner itself.
#
# Usage:  bash tools/wayland_scan.sh [path-to-wayland-protocols-dir]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/include/wayland-generated"

if ! command -v wayland-scanner &>/dev/null; then
    echo "[wayland_scan] ERROR: wayland-scanner not found. Install libwayland-dev." >&2
    exit 1
fi

mkdir -p "${OUT_DIR}"

WAYLAND_SHARE="${1:-/usr/share/wayland}"
PROTOCOLS_DIR="${2:-/usr/share/wayland-protocols}"

scan() {
    local xml="$1"
    local stem
    stem="$(basename "${xml%.xml}")"
    echo "[wayland_scan] ${stem}"
    wayland-scanner client-header  "${xml}" "${OUT_DIR}/${stem}-client-protocol.h"
    wayland-scanner server-header  "${xml}" "${OUT_DIR}/${stem}-server-protocol.h"
    wayland-scanner private-code   "${xml}" "${OUT_DIR}/${stem}-protocol.c"
}

# Core Wayland protocol
if [[ -f "${WAYLAND_SHARE}/wayland.xml" ]]; then
    scan "${WAYLAND_SHARE}/wayland.xml"
fi

# XDG shell
if [[ -f "${PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" ]]; then
    scan "${PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
fi

# linux-dmabuf
if [[ -f "${PROTOCOLS_DIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml" ]]; then
    scan "${PROTOCOLS_DIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml"
fi

# viewporter
if [[ -f "${PROTOCOLS_DIR}/stable/viewporter/viewporter.xml" ]]; then
    scan "${PROTOCOLS_DIR}/stable/viewporter/viewporter.xml"
fi

echo "[wayland_scan] Done. Headers written to ${OUT_DIR}/"
