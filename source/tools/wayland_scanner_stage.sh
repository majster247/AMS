#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
WAYLAND_DIR="${STAGE_DIR}/wayland"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
GENERATED_DIR="${STAGE_DIR}/generated"

if [[ ! -d "${WAYLAND_DIR}/.git" ]]; then
    echo "[wayland-scanner] Wayland sources not staged. Run 'make wayland_stage' first."
    exit 1
fi

echo "[wayland-scanner] Building wayland-scanner from upstream sources..."

HOST_BUILD="${STAGE_DIR}/build-scanner"
if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
    if [[ ! -f "${HOST_BUILD}/wayland-scanner" ]]; then
        meson setup "${HOST_BUILD}" "${WAYLAND_DIR}" \
            -Dtests=false -Ddocumentation=false -Ddtd_validation=false \
            2>/dev/null || true
        ninja -C "${HOST_BUILD}" wayland-scanner 2>/dev/null || true
    fi
fi

SCANNER=""
if [[ -x "${HOST_BUILD}/wayland-scanner" ]]; then
    SCANNER="${HOST_BUILD}/wayland-scanner"
elif command -v wayland-scanner >/dev/null 2>&1; then
    SCANNER="$(command -v wayland-scanner)"
fi

if [[ -z "${SCANNER}" ]]; then
    echo "[wayland-scanner] No wayland-scanner found. Install wayland-devel or build from source."
    echo "  Fallback: using pre-existing protocol stubs from ams_wl_compositor.c"
    exit 0
fi

echo "[wayland-scanner] Using scanner: ${SCANNER}"
mkdir -p "${GENERATED_DIR}"

generate() {
    local xml="$1"
    local base
    base="$(basename "${xml}" .xml)"
    echo "  Generating ${base}-protocol.h and ${base}-protocol.c ..."
    "${SCANNER}" server-header "${xml}" "${GENERATED_DIR}/${base}-protocol.h"
    "${SCANNER}" private-code  "${xml}" "${GENERATED_DIR}/${base}-protocol.c"
    "${SCANNER}" client-header "${xml}" "${GENERATED_DIR}/${base}-client-protocol.h"
}

# Core Wayland protocol
if [[ -f "${WAYLAND_DIR}/protocol/wayland.xml" ]]; then
    generate "${WAYLAND_DIR}/protocol/wayland.xml"
fi

# XDG shell
XDG_SHELL_XML=""
for f in \
    "${PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" \
    "${PROTOCOLS_DIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml"; do
    if [[ -f "$f" ]]; then XDG_SHELL_XML="$f"; break; fi
done
if [[ -n "${XDG_SHELL_XML}" ]]; then
    generate "${XDG_SHELL_XML}"
fi

# linux-dmabuf
DMABUF_XML=""
for f in \
    "${PROTOCOLS_DIR}/stable/linux-dmabuf/linux-dmabuf-v1.xml" \
    "${PROTOCOLS_DIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml"; do
    if [[ -f "$f" ]]; then DMABUF_XML="$f"; break; fi
done
if [[ -n "${DMABUF_XML}" ]]; then
    generate "${DMABUF_XML}"
fi

# xdg-decoration
DECOR_XML="${PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
if [[ -f "${DECOR_XML}" ]]; then
    generate "${DECOR_XML}"
fi

echo "[wayland-scanner] Generated protocol files in: ${GENERATED_DIR}"
ls -la "${GENERATED_DIR}/" 2>/dev/null || true
