#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/external/graphics-stack"
MANIFEST="${OUT_DIR}/manifest.txt"

mkdir -p "${OUT_DIR}"

bash "${ROOT_DIR}/tools/wayland_stage.sh"
bash "${ROOT_DIR}/tools/mesa_stage.sh"

{
  echo "graphics_stack_stage:"
  echo "  wayland_manifest=${ROOT_DIR}/external/wayland-stack/manifest.txt"
  echo "  mesa_manifest=${ROOT_DIR}/external/mesa-stack/manifest.txt"
  echo "  runtime_apps=${ROOT_DIR}/external/wayland-stack/ams-src"
} > "${MANIFEST}"

echo "[graphics-stack-stage] manifest written to ${MANIFEST}"
