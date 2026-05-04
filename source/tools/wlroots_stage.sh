#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/wlroots-stack"
mkdir -p "${DEPS_DIR}"
if [[ ! -d "${DEPS_DIR}/wlroots/.git" ]]; then
  git clone --depth 1 https://github.com/swaywm/wlroots.git "${DEPS_DIR}/wlroots"
fi
echo "[wlroots-stage] cloned to ${DEPS_DIR}/wlroots (needs DRM/EGL backends for AMS)"
