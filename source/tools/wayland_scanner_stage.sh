#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
GEN_DIR="${STAGE_DIR}/generated"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
WAYLAND_DIR="${STAGE_DIR}/wayland"

# Ensure upstream sources are present
if [[ ! -d "${WAYLAND_DIR}/.git" ]] || [[ ! -d "${PROTOCOLS_DIR}/.git" ]]; then
    echo "[scanner] Running wayland_stage.sh first..."
    bash "${ROOT_DIR}/tools/wayland_stage.sh"
fi

mkdir -p "${GEN_DIR}"

# Try to find wayland-scanner on host
SCANNER=""
if command -v wayland-scanner >/dev/null 2>&1; then
    SCANNER="wayland-scanner"
elif [[ -x "${STAGE_DIR}/build-scanner/wayland-scanner" ]]; then
    SCANNER="${STAGE_DIR}/build-scanner/wayland-scanner"
else
    echo "[scanner] wayland-scanner not found on host."
    echo "[scanner] Attempting to build from source..."
    if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
        BUILD="${STAGE_DIR}/build-scanner"
        meson setup "${BUILD}" "${WAYLAND_DIR}" \
            -Dscanner=true -Dlibraries=false -Dtests=false -Ddocumentation=false \
            2>/dev/null || true
        ninja -C "${BUILD}" wayland-scanner 2>/dev/null || true
        if [[ -x "${BUILD}/wayland-scanner" ]]; then
            SCANNER="${BUILD}/wayland-scanner"
        fi
    fi
fi

if [[ -z "${SCANNER}" ]]; then
    echo "[scanner] WARNING: Could not obtain wayland-scanner."
    echo "[scanner] Generated headers will not be available."
    echo "[scanner] Install wayland-scanner or meson+ninja to enable."
    exit 0
fi

echo "[scanner] Using: ${SCANNER}"

generate() {
    local xml="$1"
    local base="$(basename "${xml}" .xml)"
    echo "  generating ${base}..."
    "${SCANNER}" server-header "${xml}" "${GEN_DIR}/${base}-server-protocol.h" 2>/dev/null || true
    "${SCANNER}" client-header "${xml}" "${GEN_DIR}/${base}-client-protocol.h" 2>/dev/null || true
    "${SCANNER}" private-code  "${xml}" "${GEN_DIR}/${base}-protocol.c" 2>/dev/null || true
}

# Core Wayland protocol
if [[ -f "${WAYLAND_DIR}/protocol/wayland.xml" ]]; then
    generate "${WAYLAND_DIR}/protocol/wayland.xml"
fi

# Stable protocols
for xml in "${PROTOCOLS_DIR}"/stable/*/*.xml; do
    [[ -f "${xml}" ]] && generate "${xml}"
done

# Staging protocols
for xml in "${PROTOCOLS_DIR}"/staging/*/*.xml; do
    [[ -f "${xml}" ]] && generate "${xml}"
done

# Unstable protocols (key ones for wlroots)
for proto in xdg-shell linux-dmabuf pointer-constraints relative-pointer; do
    for xml in "${PROTOCOLS_DIR}"/unstable/${proto}/*.xml; do
        [[ -f "${xml}" ]] && generate "${xml}"
    done
done

echo "[scanner] Generated headers in: ${GEN_DIR}"
ls -la "${GEN_DIR}/" 2>/dev/null || true
