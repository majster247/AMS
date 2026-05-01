#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/toolchain"
BINUTILS_DIR="${DEPS_DIR}/binutils-gdb"
GCC_DIR="${DEPS_DIR}/gcc"
BUILD_DIR="${DEPS_DIR}/build-x86_64-elf"
PREFIX_DIR="${DEPS_DIR}/prefix"
TARGET="x86_64-elf"

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

clone_or_update "https://github.com/bminor/binutils-gdb.git" "${BINUTILS_DIR}"
clone_or_update "https://github.com/gcc-mirror/gcc.git" "${GCC_DIR}"

echo "[gcc-stage] repositories ready:"
echo "  - ${BINUTILS_DIR}"
echo "  - ${GCC_DIR}"

if command -v make >/dev/null 2>&1 && command -v gcc >/dev/null 2>&1; then
  mkdir -p "${BUILD_DIR}" "${PREFIX_DIR}"

  rm -rf "${BUILD_DIR}/binutils" "${BUILD_DIR}/gcc"
  mkdir -p "${BUILD_DIR}/binutils" "${BUILD_DIR}/gcc"

  pushd "${BUILD_DIR}/binutils" >/dev/null
  "${BINUTILS_DIR}/configure" --target="${TARGET}" --prefix="${PREFIX_DIR}" --disable-nls --disable-werror
  make -j"$(nproc)"
  make install
  popd >/dev/null

  pushd "${BUILD_DIR}/gcc" >/dev/null
  "${GCC_DIR}/configure" --target="${TARGET}" --prefix="${PREFIX_DIR}" --disable-nls --enable-languages=c,c++ --without-headers
  make -j"$(nproc)" all-gcc all-target-libgcc
  make install-gcc install-target-libgcc
  popd >/dev/null

  "${PREFIX_DIR}/bin/${TARGET}-gcc" --version | head -n 1
  echo "[gcc-stage] cross toolchain installed in ${PREFIX_DIR}"
else
  echo "[gcc-stage] host make/gcc unavailable, skipped local build."
fi
