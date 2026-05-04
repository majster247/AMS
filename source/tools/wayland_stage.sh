#!/usr/bin/env bash
#
# wayland_stage.sh - clone upstream Wayland-desktop graphics stack into
# external/wayland-stack/. Pinned to known-good tags so subsequent
# `meson configure` builds are reproducible.
#
# Components:
#   - wayland (libwayland + wayland-scanner)
#   - wayland-protocols
#   - libxkbcommon
#   - libinput
#   - pixman
#   - cairo
#   - libffi
#   - mlibc                  (used as the AMS-OS userspace libc target)
#   - mesa3d                 (EGL + GBM software stack, swrast / llvmpipe)
#   - wlroots                (Smithay-style modular compositor library)
#
# Build of these components is driven by Makefile targets (wayland_build,
# mesa_build, wlroots_build). This script only fetches the sources.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack"
mkdir -p "${STAGE_DIR}"

WAYLAND_TAG="${WAYLAND_TAG:-1.23.0}"
WAYLAND_PROTOCOLS_TAG="${WAYLAND_PROTOCOLS_TAG:-1.36}"
XKBCOMMON_TAG="${XKBCOMMON_TAG:-xkbcommon-1.7.0}"
LIBINPUT_TAG="${LIBINPUT_TAG:-1.26.1}"
PIXMAN_TAG="${PIXMAN_TAG:-pixman-0.44.0}"
CAIRO_TAG="${CAIRO_TAG:-1.18.2}"
LIBFFI_TAG="${LIBFFI_TAG:-v3.4.6}"
MLIBC_TAG="${MLIBC_TAG:-master}"
MESA_TAG="${MESA_TAG:-mesa-24.2.4}"
WLROOTS_TAG="${WLROOTS_TAG:-0.18.1}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  local ref="$3"
  if [[ ! -d "${dst}/.git" ]]; then
    git clone --depth 50 "${repo_url}" "${dst}"
  else
    git -C "${dst}" fetch --depth 50 origin
  fi
  if [[ -n "${ref}" ]]; then
    git -C "${dst}" -c advice.detachedHead=false checkout "${ref}" || \
      git -C "${dst}" -c advice.detachedHead=false checkout "tags/${ref}" || \
      git -C "${dst}" -c advice.detachedHead=false checkout "origin/${ref}"
  fi
}

clone_or_update https://gitlab.freedesktop.org/wayland/wayland.git              "${STAGE_DIR}/wayland"            "${WAYLAND_TAG}"
clone_or_update https://gitlab.freedesktop.org/wayland/wayland-protocols.git    "${STAGE_DIR}/wayland-protocols"  "${WAYLAND_PROTOCOLS_TAG}"
clone_or_update https://github.com/xkbcommon/libxkbcommon.git                   "${STAGE_DIR}/libxkbcommon"       "${XKBCOMMON_TAG}"
clone_or_update https://gitlab.freedesktop.org/libinput/libinput.git            "${STAGE_DIR}/libinput"           "${LIBINPUT_TAG}"
clone_or_update https://gitlab.freedesktop.org/pixman/pixman.git                "${STAGE_DIR}/pixman"             "${PIXMAN_TAG}"
clone_or_update https://gitlab.freedesktop.org/cairo/cairo.git                  "${STAGE_DIR}/cairo"              "${CAIRO_TAG}"
clone_or_update https://github.com/libffi/libffi.git                            "${STAGE_DIR}/libffi"             "${LIBFFI_TAG}"
clone_or_update https://github.com/managarm/mlibc.git                           "${STAGE_DIR}/mlibc"              "${MLIBC_TAG}"
clone_or_update https://gitlab.freedesktop.org/mesa/mesa.git                    "${STAGE_DIR}/mesa"               "${MESA_TAG}"
clone_or_update https://gitlab.freedesktop.org/wlroots/wlroots.git              "${STAGE_DIR}/wlroots"            "${WLROOTS_TAG}"

echo "[wayland-stage] All upstream components staged at ${STAGE_DIR}"
ls -1 "${STAGE_DIR}"
