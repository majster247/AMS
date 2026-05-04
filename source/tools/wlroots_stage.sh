#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wlroots-stack"
WLROOTS_DIR="${STAGE_DIR}/wlroots"

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

clone_or_update "https://github.com/swaywm/wlroots.git" "${WLROOTS_DIR}"

echo "[wlroots-stage] Repository ready:"
echo "  - ${WLROOTS_DIR}"

# Copy AMS backend into wlroots tree
AMS_BACKEND_DIR="${WLROOTS_DIR}/backend/ams"
mkdir -p "${AMS_BACKEND_DIR}"

if [[ -f "${ROOT_DIR}/src/apps/wayland/wlroots_backend_ams.c" ]]; then
    cp "${ROOT_DIR}/src/apps/wayland/wlroots_backend_ams.c" "${AMS_BACKEND_DIR}/"
    echo "[wlroots-stage] AMS backend copied to ${AMS_BACKEND_DIR}/"
fi

echo "[wlroots-stage] Done."
echo ""
echo "To build wlroots for AMS, the following AMS libraries must be available:"
echo "  - libdrm (DRM ioctl layer in kernel)"
echo "  - libinput (src/lib/libinput/)"
echo "  - pixman (src/lib/pixman/)"
echo "  - libffi (src/lib/libffi_stubs.c)"
echo "  - GBM (src/lib/gbm_ams.c)"
echo "  - wayland-server (from external/wayland-stack/)"
echo ""
echo "Cross-compilation with x86_64-elf-gcc using the AMS libc is required."
