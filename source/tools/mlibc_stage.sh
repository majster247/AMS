#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mlibc-stack"
MLIBC_DIR="${DEPS_DIR}/mlibc"

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

echo "[mlibc-stage] Cloning/updating mlibc..."
clone_or_update "https://github.com/managarm/mlibc.git" "${MLIBC_DIR}"

echo "[mlibc-stage] repository ready: ${MLIBC_DIR}"
echo ""
echo "  mlibc is a lightweight, portable C library designed for hobby OS projects."
echo "  AMS syscall layer is already partially mlibc-compatible (see linux_syscalls.h)."
echo ""
echo "  Integration steps:"
echo "    1. Create an AMS sysdep layer in mlibc (options/ams-sysdeps/)"
echo "    2. Map AMS syscall numbers to mlibc's generic-syscall interface"
echo "    3. Build with: meson setup build -Dlinux_kernel_headers=${ROOT_DIR}/include"
echo "    4. Replace src/lib/* with mlibc objects for full POSIX support"
echo ""
echo "  Current AMS libc (src/lib/*) provides: crt0, syscall, malloc, string, stdio, setjmp"
echo "  mlibc would add: pthreads, dlfcn, locale, math, networking, full POSIX headers"
