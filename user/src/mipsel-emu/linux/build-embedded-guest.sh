#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../../.." && pwd)

linux_dir=${LINUX_DIR:-"$repo_root/tools/linux"}
kernel_out=${KERNEL_OUTPUT_DIR:-"$repo_root/build/linux-embedded"}
rootfs_out=${OUTPUT_DIR:-"$repo_root/build/mipsel-emu-embedded-rootfs"}
dtb_out=${DTB_OUTPUT:-"$kernel_out/mipsel-emu-embedded.dtb"}
cross_compile=${CROSS_COMPILE:-"$repo_root/tools/mipsel-linux-musl/bin/mipsel-unknown-linux-musl-"}
jobs=${JOBS:-4}
kernel_fragment=${KERNEL_FRAGMENT:-"$script_dir/emu.config"}
dts_file=${DTS_FILE:-"$script_dir/mipsel-emu-embedded.dts"}
make_path=${MAKE:-make}

if [ ! -d "$linux_dir" ] || [ ! -f "$linux_dir/Makefile" ]; then
    echo "missing Linux source tree: $linux_dir" >&2
    exit 1
fi
if [ ! -f "$kernel_fragment" ]; then
    echo "missing kernel fragment: $kernel_fragment" >&2
    exit 1
fi
if ! command -v "$cross_compile"gcc >/dev/null 2>&1; then
    echo "missing MIPS compiler: ${cross_compile}gcc" >&2
    exit 1
fi
if ! command -v "$make_path" >/dev/null 2>&1; then
    echo "missing make: $make_path" >&2
    exit 1
fi
if ! command -v dtc >/dev/null 2>&1; then
    echo "missing device-tree compiler: dtc" >&2
    exit 1
fi
if [ ! -f "$dts_file" ]; then
    echo "missing embedded device tree source: $dts_file" >&2
    exit 1
fi

mkdir -p "$kernel_out"
if [ ! -f "$kernel_out/.config" ]; then
    "$make_path" -s -C "$linux_dir" O="$kernel_out" ARCH=mips \
        CROSS_COMPILE="$cross_compile" tinyconfig
fi

"$linux_dir/scripts/kconfig/merge_config.sh" -m -O "$kernel_out" \
    "$kernel_out/.config" "$kernel_fragment"
"$make_path" -s -C "$linux_dir" O="$kernel_out" ARCH=mips \
    CROSS_COMPILE="$cross_compile" olddefconfig
"$make_path" -s -C "$linux_dir" O="$kernel_out" ARCH=mips \
    CROSS_COMPILE="$cross_compile" -j"$jobs" vmlinux
dtc -I dts -O dtb -o "$dtb_out" "$dts_file"

BUSYBOX_CONFIG="$script_dir/busybox-embedded.config" \
KERNEL_CONFIG="$kernel_out/.config" \
OUTPUT_DIR="$rootfs_out" \
    "$script_dir/build-initramfs.sh"

echo "headless MIPS kernel: $kernel_out/vmlinux"
echo "headless MIPS DTB:    $dtb_out"
echo "headless initramfs:    $rootfs_out/initramfs.cpio.gz"
