#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/graphics-stack"
MLIBC_DIR="${DEPS_DIR}/mlibc"
LIBFFI_DIR="${DEPS_DIR}/libffi"

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

clone_or_update "https://github.com/managarm/mlibc.git" "${MLIBC_DIR}"
clone_or_update "https://github.com/libffi/libffi.git" "${LIBFFI_DIR}"

echo "[mlibc-stage] repositories ready:"
echo "  - ${MLIBC_DIR}"
echo "  - ${LIBFFI_DIR}"
