#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/external/wayland-stack"
REPORT="${OUT_DIR}/ams-port-preflight.txt"

mkdir -p "${OUT_DIR}"

check_rg() {
  local label="$1"
  local pattern="$2"
  shift 2
  local paths=("$@")
  if [[ "${#paths[@]}" -eq 0 ]]; then
    paths=("${ROOT_DIR}")
  fi
  if rg -q "${pattern}" "${paths[@]}"; then
    printf "[ok]   %s\n" "${label}"
  else
    printf "[miss] %s\n" "${label}"
  fi
}

{
  echo "AMS Wayland/wlroots port preflight"
  echo "=================================="
  echo
  echo "Kernel/userspace ABI:"
  check_rg "AF_UNIX sockets" "SYS_SOCKET|sys_socket|AF_UNIX" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "sendmsg/recvmsg with SCM_RIGHTS" "SYS_SENDMSG|SYS_RECVMSG|SCM_RIGHTS|sys_sendmsg|sys_recvmsg" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "poll/ppoll" "SYS_POLL|SYS_PPOLL|sys_poll|sys_ppoll" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "epoll" "SYS_EPOLL_CREATE1|SYS_EPOLL_CTL|SYS_EPOLL_WAIT|sys_epoll" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "memfd shared memory" "SYS_MEMFD_CREATE|sys_memfd_create|memfd_create" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "mmap/munmap" "SYS_MMAP|sys_mmap|mmap\\(" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "POSIX shm_open" "shm_open" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "DRM/KMS ioctls" "DRM_IOCTL|DRM_MODE|drmMode|/dev/dri|KMS_" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  check_rg "GEM/TTM buffer objects" "\\bGEM\\b|\\bTTM\\b|drm_gem|ttm_" "${ROOT_DIR}/src" "${ROOT_DIR}/include"
  echo
  echo "Staged source dependencies:"
  for dep in wayland wayland-protocols wlroots pixman cairo libinput libffi mlibc; do
    if [[ -d "${OUT_DIR}/${dep}/.git" ]]; then
      printf "[ok]   %s: " "${dep}"
      git -C "${OUT_DIR}/${dep}" rev-parse --short HEAD
    else
      printf "[miss] %s\n" "${dep}"
    fi
  done
  if [[ -d "${ROOT_DIR}/external/mesa-stack/mesa/.git" ]]; then
    printf "[ok]   mesa: "
    git -C "${ROOT_DIR}/external/mesa-stack/mesa" rev-parse --short HEAD
  else
    printf "[miss] mesa\n"
  fi
  if [[ -d "${ROOT_DIR}/external/mesa-stack/libdrm/.git" ]]; then
    printf "[ok]   libdrm: "
    git -C "${ROOT_DIR}/external/mesa-stack/libdrm" rev-parse --short HEAD
  else
    printf "[miss] libdrm\n"
  fi
  echo
  echo "Expected hard blockers before native wlroots:"
  echo "- real DRM/KMS character device and mode-setting ioctl ABI"
  echo "- GEM/TTM or equivalent dma-buf-capable buffer object manager"
  echo "- file-backed MAP_SHARED/memfd mappings visible across processes"
  echo "- shm_open wrapper or mlibc route to memfd-backed shared memory"
  echo "- libffi closures/calls for wlroots dependencies"
  echo "- input event device ABI for libinput"
} | tee "${REPORT}"

echo "[wayland-preflight] wrote ${REPORT}"
