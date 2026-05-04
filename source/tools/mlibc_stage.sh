#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/mlibc-stage"
MLIBC_DIR="${STAGE_DIR}/mlibc"

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

clone_or_update "https://github.com/managarm/mlibc.git" "${MLIBC_DIR}"

echo "[mlibc-stage] repository ready: ${MLIBC_DIR}"
echo "[mlibc-stage] AMS sysdeps live in src/lib/posix/mlibc_sysdeps.c"
echo "[mlibc-stage] follow-up PR: meson cross-build with sysdeps/ams"
