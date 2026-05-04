#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/cairo-stack"
CAIRO_DIR="${DEPS_DIR}/cairo"

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

clone_or_update "https://gitlab.freedesktop.org/cairo/cairo.git" "${CAIRO_DIR}"

echo "[cairo-stage] repository ready: ${CAIRO_DIR}"
echo "[cairo-stage] Cairo requires pixman, fontconfig (optional), freetype (optional)"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  rm -rf "${DEPS_DIR}/build"
  meson setup "${DEPS_DIR}/build" "${CAIRO_DIR}" \
    -Ddefault_library=static \
    -Dxlib=disabled \
    -Dxcb=disabled \
    -Dquartz=disabled \
    -Dpng=disabled \
    -Dfreetype=disabled \
    -Dfontconfig=disabled \
    -Dtests=disabled \
    -Dspectre=disabled || true
  ninja -C "${DEPS_DIR}/build" -j"$(nproc)" || echo "[cairo-stage] build may need cross setup"
  echo "[cairo-stage] build attempted"
else
  echo "[cairo-stage] meson/ninja unavailable, source-only stage."
fi
