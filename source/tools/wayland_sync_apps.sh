#!/usr/bin/env bash
#
# wayland_sync_apps.sh - copy the AMS-OS Wayland userspace sources into
# the staging directory used by the meson cross builds. Replaces the
# legacy sync script that staged the deleted hand-rolled compositor.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${ROOT_DIR}/src/apps/wayland"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack/ams-src"

mkdir -p "${STAGE_DIR}"

apps=(
  "ams_compositor.c"
  "ams_session.c"
  "ams_smoke_client.c"
  "ams_egl_smoke.c"
)

for app in "${apps[@]}"; do
  cp "${SRC_DIR}/${app}" "${STAGE_DIR}/${app}"
done

echo "[wayland-sync] AMS Wayland sources synced to ${STAGE_DIR}"
ls -1 "${STAGE_DIR}"
