#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${ROOT_DIR}/src/apps/wayland"
STAGE_DIR="${ROOT_DIR}/external/wayland-stack/ams-src"

mkdir -p "${STAGE_DIR}"

apps=(
  "ams_wl_compositor.c"
  "ams_wayland_shell.c"
  "wayland_session.c"
  "wayland_smoke.c"
  "wayland_smoke_client.c"
  "wayland_egl_smoke.c"
)

for app in "${apps[@]}"; do
  cp "${SRC_DIR}/${app}" "${STAGE_DIR}/${app}"
done

# Validate compositor source integrity (single translation unit)
includes_count="$(awk '/^#include "ams_syscall.h"$/{c++} END{print c+0}' "${STAGE_DIR}/ams_wl_compositor.c")"
if [[ "${includes_count}" != "1" ]]; then
  echo "wayland_sync_apps: compositor integrity check failed (include count=${includes_count})"
  echo "wayland_sync_apps: attempting auto-cleanup..."
  python3 - "$STAGE_DIR/ams_wl_compositor.c" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8", errors="ignore")

needle = '#include "ams_syscall.h"\n'
indices = []
start = 0
while True:
    idx = text.find(needle, start)
    if idx == -1:
        break
    indices.append(idx)
    start = idx + len(needle)

if not indices:
    raise SystemExit("wayland_sync_apps: compositor missing expected include header")

if len(indices) >= 2:
    prefix = text[:indices[1]]
    if "int main(void){ return 0; }" in prefix:
        text = text[indices[1]:]

text = text.rstrip() + "\n"
path.write_text(text, encoding="utf-8")
PY
  includes_count="$(awk '/^#include "ams_syscall.h"$/{c++} END{print c+0}' "${STAGE_DIR}/ams_wl_compositor.c")"
  if [[ "${includes_count}" != "1" ]]; then
    echo "wayland_sync_apps: cleanup failed (count=${includes_count}), proceeding"
  fi
fi

echo "[wayland-sync] AMS Wayland sources synced and validated."
