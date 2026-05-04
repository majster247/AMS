#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/pixman-stack"
PIXMAN_DIR="${DEPS_DIR}/pixman"

mkdir -p "${DEPS_DIR}"

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

clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "${PIXMAN_DIR}"

echo "[pixman-stage] repository ready: ${PIXMAN_DIR}"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${DEPS_DIR}/build"
  meson setup "${DEPS_DIR}/build" "${PIXMAN_DIR}" \
    -Ddefault_library=static \
    -Dgtk=disabled \
    -Dlibpng=disabled \
    -Dtests=disabled \
    -Ddemos=disabled || true
  ninja -C "${DEPS_DIR}/build" -j"$(nproc)" || echo "[pixman-stage] build may need cross setup"
  echo "[pixman-stage] build attempted"
else
  echo "[pixman-stage] meson/ninja unavailable, source-only stage."
fi
