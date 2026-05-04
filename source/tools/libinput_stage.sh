#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/libinput-stack"
LIBINPUT_DIR="${DEPS_DIR}/libinput"
LIBEVDEV_DIR="${DEPS_DIR}/libevdev"

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

clone_or_update "https://gitlab.freedesktop.org/libevdev/libevdev.git" "${LIBEVDEV_DIR}"
clone_or_update "https://gitlab.freedesktop.org/libinput/libinput.git" "${LIBINPUT_DIR}"

echo "[libinput-stage] repositories ready:"
echo "  - ${LIBEVDEV_DIR}"
echo "  - ${LIBINPUT_DIR}"

if command -v meson >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
  # Build libevdev first
  rm -rf "${DEPS_DIR}/build-evdev"
  meson setup "${DEPS_DIR}/build-evdev" "${LIBEVDEV_DIR}" \
    -Ddefault_library=static \
    -Dtests=disabled \
    -Ddocumentation=disabled || true
  ninja -C "${DEPS_DIR}/build-evdev" -j"$(nproc)" || echo "[libinput-stage] libevdev build may need setup"

  # Then libinput
  rm -rf "${DEPS_DIR}/build"
  meson setup "${DEPS_DIR}/build" "${LIBINPUT_DIR}" \
    -Ddefault_library=static \
    -Ddebug-gui=false \
    -Dtests=false \
    -Ddocumentation=false \
    -Dlibwacom=false || true
  ninja -C "${DEPS_DIR}/build" -j"$(nproc)" || echo "[libinput-stage] build may need cross setup"
  echo "[libinput-stage] build attempted"
else
  echo "[libinput-stage] meson/ninja unavailable, source-only stage."
fi
