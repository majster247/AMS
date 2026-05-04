#!/usr/bin/env bash
# wayland_sync_apps.sh – Sync Wayland test apps to the staging directory.
#
# Note: the main compositor (ams_compositor.c) is built directly from source.
# This script only handles the IPC smoke-test helpers.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${ROOT_DIR}/src/apps/wayland"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack/ams-src"

mkdir -p "${STAGE_DIR}"

# Smoke-test binaries (kept for regression testing)
apps=(
  "wayland_session.c"
  "wayland_smoke.c"
  "wayland_smoke_client.c"
  "wayland_egl_smoke.c"
)

for app in "${apps[@]}"; do
  if [[ -f "${SRC_DIR}/${app}" ]]; then
    cp "${SRC_DIR}/${app}" "${STAGE_DIR}/${app}"
    echo "[wayland-sync] synced: ${app}"
  fi
done

echo "[wayland-sync] AMS Wayland smoke-test sources synced."
