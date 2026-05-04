#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/src/apps/wayland/generated"

if ! command -v wayland-scanner >/dev/null 2>&1; then
  echo "regenerate_wayland_protocol: install libwayland-bin (wayland-scanner) on the host" >&2
  exit 1
fi

bash "${ROOT_DIR}/tools/wayland_stage.sh"

WL_XML="${ROOT_DIR}/external/wayland-stack/wayland/protocol/wayland.xml"
XDG_XML="${ROOT_DIR}/external/wayland-stack/wayland-protocols/stable/xdg-shell/xdg-shell.xml"

mkdir -p "${OUT_DIR}"

wayland-scanner server-header "${WL_XML}" "${OUT_DIR}/wayland-server-protocol.h"
wayland-scanner client-header "${WL_XML}" "${OUT_DIR}/wayland-client-protocol.h"
wayland-scanner private-code "${WL_XML}" "${OUT_DIR}/wayland-protocol.c"
wayland-scanner server-header "${XDG_XML}" "${OUT_DIR}/xdg-shell-server-protocol.h"
wayland-scanner client-header "${XDG_XML}" "${OUT_DIR}/xdg-shell-client-protocol.h"
wayland-scanner private-code "${XDG_XML}" "${OUT_DIR}/xdg-shell-protocol.c"

echo "[regenerate-wayland-protocol] done -> ${OUT_DIR}"
