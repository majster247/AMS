#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
WAYLAND_DIR="${STAGE_DIR}/wayland"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
WLR_PORT_DIR="${STAGE_DIR}/wlroots-port"

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

mkdir -p "${WLR_PORT_DIR}"
cat > "${WLR_PORT_DIR}/README.md" <<'EOF'
# AMS wlroots compositor port

This directory is prepared by `tools/wayland_stage.sh` and consumed by
`programs/wayland/wlroots/bin/ams-wlroots-compositor`.

Expected host-side port flow:

1. Run wayland-scanner on core + xdg shell protocols.
2. Build wlroots with libinput + pixman + cairo and the DRM backend.
3. Install the compositor binary under:
   - `programs/wayland/wlroots/bin/ams-wlroots-compositor`
4. Keep runtime metadata in this directory for quick diagnostics.
EOF

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"
echo "  - ${WLR_PORT_DIR}"
