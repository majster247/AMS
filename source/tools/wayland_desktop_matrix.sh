#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/build/tests"
OUT_FILE="${OUT_DIR}/wayland_desktop_matrix.txt"

mkdir -p "${OUT_DIR}"

check_bin() {
  local rel="$1"
  if [[ -s "${ROOT_DIR}/${rel}" ]]; then
    echo "[ok] ${rel}" >> "${OUT_FILE}"
  else
    echo "[missing] ${rel}" >> "${OUT_FILE}"
    return 1
  fi
}

echo "Wayland desktop compatibility matrix" > "${OUT_FILE}"
echo "generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${OUT_FILE}"
echo >> "${OUT_FILE}"

status=0
check_bin "build/ams_wayland_shell.elf" || status=1
check_bin "build/wayland_smoke.elf" || status=1
check_bin "build/wayland_smoke_client.elf" || status=1
check_bin "build/wayland_egl_smoke.elf" || status=1

echo >> "${OUT_FILE}"
if [[ ${status} -eq 0 ]]; then
  echo "result: PASS" >> "${OUT_FILE}"
  echo "[matrix] PASS - ${OUT_FILE}"
else
  echo "result: FAIL" >> "${OUT_FILE}"
  echo "[matrix] FAIL - ${OUT_FILE}"
  exit 1
fi
