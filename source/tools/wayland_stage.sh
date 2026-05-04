#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
WAYLAND_DIR="${STAGE_DIR}/wayland"
PROTOCOLS_DIR="${STAGE_DIR}/wayland-protocols"
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

clone_or_update "https://github.com/wayland-mirror/wayland.git" "${WAYLAND_DIR}"
clone_or_update "https://github.com/wayland-mirror/wayland-protocols.git" "${PROTOCOLS_DIR}"
clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}"

echo "[wayland-stage] repositories ready:"
echo "  - ${WAYLAND_DIR}"
echo "  - ${PROTOCOLS_DIR}"
echo "  - ${WLROOTS_DIR}"
