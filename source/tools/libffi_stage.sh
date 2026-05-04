#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/libffi-stack"
LIBFFI_DIR="${DEPS_DIR}/libffi"
LIBFFI_BUILD="${DEPS_DIR}/build"

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

clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"

echo "[libffi-stage] repository ready: ${LIBFFI_DIR}"

if command -v autoreconf >/dev/null 2>&1; then
  cd "${LIBFFI_DIR}"
  if [[ ! -f configure ]]; then
    autoreconf -fi
  fi
  mkdir -p "${LIBFFI_BUILD}"
  cd "${LIBFFI_BUILD}"
  "${LIBFFI_DIR}/configure" \
    --host=x86_64-elf \
    --prefix="${DEPS_DIR}/prefix" \
    --disable-shared \
    --enable-static \
    CFLAGS="-ffreestanding -O2 -nostdlib" || true
  make -j"$(nproc)" || echo "[libffi-stage] build requires cross-toolchain"
  echo "[libffi-stage] build attempted: ${LIBFFI_BUILD}"
else
  echo "[libffi-stage] autoreconf unavailable, source-only stage."
fi
