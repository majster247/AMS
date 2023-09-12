#!/bin/sh
export PATH="$HOME/opt/cross/bin:$PATH"
set -e
. ./iso.sh

qemu-system-$(./target-triplet-to-arch.sh $HOST) -cdrom myos.iso -vnc :0
