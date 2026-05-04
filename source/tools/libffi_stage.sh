#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[libffi-stage] AMS provides a built-in libffi implementation."
echo "  Header:  ${ROOT_DIR}/include/ffi.h"
echo "  Source:  ${ROOT_DIR}/src/lib/libffi/ffi.c"
echo ""
echo "  The built-in libffi supports:"
echo "    - ffi_prep_cif / ffi_call (up to 6 integer/pointer args, x86_64 SysV ABI)"
echo "    - ffi_closure_alloc / ffi_prep_closure_loc (trampoline stubs)"
echo ""
echo "  This is sufficient for wayland-server's closure dispatch mechanism."
echo "  For full upstream libffi, clone:"
echo "    git clone --depth 1 https://github.com/libffi/libffi.git \\"
echo "        ${ROOT_DIR}/external/libffi"
