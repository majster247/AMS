#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[pixman-stage] AMS provides a built-in pixman implementation."
echo "  Header:  ${ROOT_DIR}/include/pixman.h"
echo "  Source:  ${ROOT_DIR}/src/lib/pixman/pixman.c"
echo ""
echo "  The built-in pixman covers the wlroots compositing API surface:"
echo "    - pixman_image_create_bits / _no_clear / _solid_fill"
echo "    - pixman_image_composite32 (SRC, OVER, CLEAR)"
echo "    - pixman_image_fill_rectangles"
echo "    - pixman_region32_* (init, union, intersect, subtract, copy, translate)"
echo ""
echo "  For full upstream pixman, clone:"
echo "    git clone --depth 1 https://gitlab.freedesktop.org/pixman/pixman.git \\"
echo "        ${ROOT_DIR}/external/pixman"
echo ""
echo "  No external build required for basic wlroots compositing."
