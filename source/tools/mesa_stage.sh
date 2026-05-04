#!/usr/bin/env bash
#
# mesa_stage.sh - retained for backwards compatibility.
#
# The mesa staging is now part of the unified Wayland desktop staging
# at tools/wayland_stage.sh (mesa is cloned into
# external/wayland-stack/mesa). This wrapper just forwards.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${ROOT_DIR}/tools/wayland_stage.sh" "$@"
