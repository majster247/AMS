#!/usr/bin/env bash
#
# Wayland desktop compatibility matrix for AMS-OS.
# Verifies that all major components of the new Wayland stack are present
# and that their build artifacts have non-zero size.
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

check_dir() {
  local rel="$1"
  if [[ -d "${ROOT_DIR}/${rel}" ]]; then
    echo "[ok] ${rel}/ (staged)" >> "${OUT_FILE}"
  else
    echo "[missing] ${rel}/" >> "${OUT_FILE}"
  fi
}

echo "AMS-OS Wayland desktop compatibility matrix" > "${OUT_FILE}"
echo "generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${OUT_FILE}"
echo >> "${OUT_FILE}"

status=0
check_bin "build/ams-compositor.elf" || status=1
check_bin "build/ams-session.elf" || status=1
check_bin "build/ams-smoke-client.elf" || status=1
check_bin "build/ams-egl-smoke.elf" || status=1

echo >> "${OUT_FILE}"
echo "Upstream stage components (clone via tools/wayland_stage.sh):" >> "${OUT_FILE}"
for c in wayland wayland-protocols libxkbcommon libinput pixman cairo libffi mlibc mesa wlroots; do
  check_dir "external/wayland-stack/${c}"
done

echo >> "${OUT_FILE}"
if [[ ${status} -eq 0 ]]; then
  echo "result: PASS" >> "${OUT_FILE}"
  echo "[matrix] PASS - ${OUT_FILE}"
else
  echo "result: FAIL" >> "${OUT_FILE}"
  echo "[matrix] FAIL - ${OUT_FILE}"
  exit 1
fi
