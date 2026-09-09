#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
cross_compile=${CROSS_COMPILE:-"$repo_root/tools/mipsel-linux-musl/bin/mipsel-unknown-linux-musl-"}
output_dir=${OUTPUT_DIR:-"$repo_root/build/mipsel-emu-rootfs"}
manifest="$output_dir/lvgl-elf.list"

make -C "$script_dir" CROSS_COMPILE="$cross_compile" -j"${JOBS:-4}"
mkdir -p "$output_dir"
printf '/usr/bin/lvgl-demo %s 0755\n' "$script_dir/lvgl-demo" > "$manifest"

BUSYBOX_DIR=${BUSYBOX_DIR:-"$repo_root/tools/busybox-1.38.0"} \
CROSS_COMPILE="$cross_compile" \
OUTPUT_DIR="$output_dir" \
INITRAMFS_EXTRA_ELF_LIST="$manifest" \
KERNEL_CONFIG="${KERNEL_CONFIG:-$repo_root/build/linux-tiny/.config}" \
    "$repo_root/user/src/mipsel-emu/linux/build-initramfs.sh"
