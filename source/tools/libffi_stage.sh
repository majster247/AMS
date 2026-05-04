#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/libffi-stack"
mkdir -p "${DEPS_DIR}"
if [[ ! -d "${DEPS_DIR}/libffi/.git" ]]; then
  git clone --depth 1 https://github.com/libffi/libffi.git "${DEPS_DIR}/libffi"
fi
echo "[libffi-stage] cloned to ${DEPS_DIR}/libffi (build when Mesa/llvmpipe requires FFI)"
