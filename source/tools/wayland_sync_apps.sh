#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${ROOT_DIR}/src/apps/wayland"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack/ams-src"

mkdir -p "${STAGE_DIR}"

apps=(
  "wlroots_compositor.c"
  "ams_wayland_shell.c"
  "wayland_session.c"
  "wayland_smoke.c"
  "wayland_smoke_client.c"
  "wayland_egl_smoke.c"
)

for app in "${apps[@]}"; do
  cp "${SRC_DIR}/${app}" "${STAGE_DIR}/${app}"
done

echo "[wayland-sync] AMS Wayland sources synced and validated."
