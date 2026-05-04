#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wlroots-stage"
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

clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "${WLROOTS_DIR}" \
  || clone_or_update "https://github.com/swaywm/wlroots.git" "${WLROOTS_DIR}"

echo "[wlroots-stage] repository ready: ${WLROOTS_DIR}"
echo "[wlroots-stage] AMS port lives in src/lib/wlroots and overlays selected"
echo "[wlroots-stage] modules (backend/headless, backend/ams, render/pixman)."
