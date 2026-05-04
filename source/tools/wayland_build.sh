#!/usr/bin/env bash
#
# wayland_build.sh - drive the meson/ninja builds for the AMS-OS desktop
# stack. Each component is configured against the AMS sysroot
# (external/wayland-stack/sysroot) and into a per-component build dir.
#
# This script is intentionally idempotent: re-runs reconfigure if necessary
# and re-emit ninja so a subsequent invocation only rebuilds dirty targets.
#
# It expects:
#   * meson, ninja, pkg-config, x86_64-elf-gcc cross-toolchain on PATH.
#   * tools/wayland_stage.sh to have been executed first.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
SYSROOT_DIR="${STAGE_DIR}/sysroot"
BUILD_BASE="${STAGE_DIR}/build"
CROSS_FILE="${ROOT_DIR}/tools/meson-cross-ams.ini"

mkdir -p "${BUILD_BASE}" "${SYSROOT_DIR}/usr/include" "${SYSROOT_DIR}/usr/lib" \
         "${SYSROOT_DIR}/etc"

require() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[wayland-build] missing tool: $1" >&2
    exit 90
  fi
}

require git

WANT_TOOLS=(meson ninja pkg-config x86_64-elf-gcc x86_64-elf-g++ wayland-scanner)
MISSING=0
for t in "${WANT_TOOLS[@]}"; do
  if ! command -v "$t" >/dev/null 2>&1; then
    echo "[wayland-build] WARNING: $t not found, skipping its build steps."
    MISSING=$((MISSING+1))
  fi
done

if [[ "${MISSING}" -gt 0 ]]; then
  echo "[wayland-build] Some tools missing; emit only sysroot skeleton + status."
  cat > "${SYSROOT_DIR}/etc/wayland-stage.status" <<EOF
sysroot prepared but builds skipped (missing tools: ${MISSING}).
Install meson, ninja, pkg-config, wayland-scanner, x86_64-elf cross gcc/g++,
and re-run \`make wayland_build\` to compile the stack.
EOF
  exit 0
fi

build_meson() {
  local name="$1"
  local src="${STAGE_DIR}/${name}"
  local out="${BUILD_BASE}/${name}"
  shift; shift || true
  if [[ ! -d "${src}" ]]; then
    echo "[wayland-build] skipping ${name} (not staged)"
    return
  fi
  mkdir -p "${out}"
  if [[ ! -f "${out}/build.ninja" ]]; then
    meson setup "${out}" "${src}" \
      --prefix=/usr \
      --libdir=lib \
      --cross-file="${CROSS_FILE}" \
      --pkg-config-path="${SYSROOT_DIR}/usr/lib/pkgconfig" \
      "$@"
  else
    meson setup --reconfigure "${out}" "${src}" \
      --prefix=/usr --libdir=lib \
      --cross-file="${CROSS_FILE}" \
      --pkg-config-path="${SYSROOT_DIR}/usr/lib/pkgconfig" \
      "$@" || true
  fi
  DESTDIR="${SYSROOT_DIR}" ninja -C "${out}" install
}

# Dependency graph:
#   libffi -> wayland (scanner is built host-side automatically by meson)
#   libxkbcommon, libinput depend on libwayland
#   pixman -> cairo
#   wayland-protocols ships only XML, install with meson and a no-op build
#   mesa depends on libwayland, libdrm shim (we use AMS DRM ABI), pixman
#   wlroots depends on libwayland, wayland-protocols, libxkbcommon, libinput,
#           pixman, mesa (gbm/egl)
build_meson libffi             -Dtests=false
build_meson wayland            -Ddocumentation=false -Ddtd_validation=false -Dtests=false
build_meson wayland-protocols
build_meson libxkbcommon       -Denable-x11=false -Denable-docs=false -Denable-tools=false
build_meson libinput           -Ddocumentation=false -Dtests=false -Dudev-dir="${SYSROOT_DIR}/etc/udev"
build_meson pixman             -Dtests=disabled -Dgtk=disabled
build_meson cairo              -Dtests=disabled -Dxlib=disabled -Dxcb=disabled -Dquartz=disabled
build_meson mesa               -Dvulkan-drivers= \
                                -Dgallium-drivers=swrast,llvmpipe \
                                -Dplatforms=wayland \
                                -Dglx=disabled -Dosmesa=true \
                                -Dgbm=enabled -Degl=enabled -Dshared-glapi=enabled \
                                -Dllvm=disabled
build_meson wlroots            -Dxwayland=disabled -Dx11-backend=disabled \
                                -Dexamples=false

echo "[wayland-build] OK - sysroot at ${SYSROOT_DIR}"
ls -1 "${SYSROOT_DIR}/usr/lib"
