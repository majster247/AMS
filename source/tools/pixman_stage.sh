#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/pixman-stack"
mkdir -p "${DEPS_DIR}"
if [[ ! -d "${DEPS_DIR}/pixman/.git" ]]; then
  git clone --depth 1 https://gitlab.freedesktop.org/pixman/pixman.git "${DEPS_DIR}/pixman"
fi
echo "[pixman-stage] cloned to ${DEPS_DIR}/pixman (cross-build not wired; integrate via Mesa/wlroots later)"
