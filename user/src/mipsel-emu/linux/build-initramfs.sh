#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../../.." && pwd)

busybox_dir=${BUSYBOX_DIR:-"$repo_root/busybox-1.38.0"}
linux_dir=${LINUX_DIR:-"$repo_root/linux-7.1.4"}
output_dir=${OUTPUT_DIR:-"$repo_root/build/mipsel-emu-rootfs"}
cross_compile=${CROSS_COMPILE:-"$repo_root/mips32el--musl--stable-2025.08-1/bin/mipsel-buildroot-linux-musl-"}
kernel_config=${KERNEL_CONFIG:-"$linux_dir/.config"}
host_cc=${HOSTCC:-cc}
jobs=${JOBS:-4}

busybox_elf="$output_dir/busybox"
archive_raw="$output_dir/initramfs.cpio"
archive="$output_dir/initramfs.cpio.gz"
host_gen="$output_dir/gen_init_cpio"

if [ ! -f "$busybox_dir/.config" ]; then
    echo "missing BusyBox configuration: $busybox_dir/.config" >&2
    exit 1
fi
if ! cross_gcc=$(command -v "${cross_compile}gcc" 2>/dev/null); then
    echo "missing MIPS cross compiler: ${cross_compile}gcc" >&2
    exit 1
fi
if ! cross_readelf=$(command -v "${cross_compile}readelf" 2>/dev/null); then
    echo "missing MIPS readelf: ${cross_compile}readelf" >&2
    exit 1
fi
if ! host_cc_path=$(command -v "$host_cc" 2>/dev/null); then
    echo "missing host C compiler: $host_cc" >&2
    exit 1
fi
if ! make_path=$(command -v make 2>/dev/null); then
    echo "missing host build tool: make" >&2
    exit 1
fi
if ! gzip_path=$(command -v gzip 2>/dev/null); then
    echo "missing host compression tool: gzip" >&2
    exit 1
fi
if [ ! -f "$linux_dir/usr/gen_init_cpio.c" ]; then
    echo "missing Linux gen_init_cpio source: $linux_dir/usr/gen_init_cpio.c" >&2
    exit 1
fi

mkdir -p "$output_dir"
stage_dir=$(mktemp -d "$output_dir/.busybox-stage.XXXXXX")
trap 'rm -rf -- "$stage_dir"' EXIT HUP INT TERM
busybox_stage="$stage_dir/source"
busybox_build="$stage_dir/output"
stage_busybox_elf="$stage_dir/busybox"
stage_archive_raw="$stage_dir/initramfs.cpio"
stage_archive="$stage_dir/initramfs.cpio.gz"
stage_host_gen="$stage_dir/gen_init_cpio"
mkdir -p "$busybox_stage" "$busybox_build"

# An O= build refuses a source tree that already contains in-tree build
# artifacts.  Build from a disposable copy so the user's BusyBox checkout and
# configuration remain untouched.
cp -a "$busybox_dir/." "$busybox_stage/"
"$make_path" -s -C "$busybox_stage" mrproper

# Keep the user's applet selection but configure this disposable build for
# static linking.
sed \
    -e 's/^# CONFIG_STATIC is not set$/CONFIG_STATIC=y/' \
    -e 's/^CONFIG_PIE=y$/# CONFIG_PIE is not set/' \
    "$busybox_dir/.config" > "$busybox_build/.config"
if ! grep -qx 'CONFIG_STATIC=y' "$busybox_build/.config"; then
    echo "cannot enable CONFIG_STATIC in the BusyBox configuration" >&2
    exit 1
fi

"$make_path" -s -C "$busybox_stage" O="$busybox_build" ARCH=mips \
    CROSS_COMPILE="$cross_compile" oldconfig </dev/null >/dev/null

# Symlinks alone do not install an applet.  Fail here instead of producing an
# archive whose /init reaches "applet not found" only after Linux boots.
for option in \
    CONFIG_ASH=y \
    CONFIG_SH_IS_ASH=y \
    CONFIG_ASH_JOB_CONTROL=y \
    CONFIG_CTTYHACK=y \
    CONFIG_MOUNT=y \
    CONFIG_MKDIR=y \
    CONFIG_SETSID=y \
    CONFIG_SLEEP=y
do
    if ! grep -qx "$option" "$busybox_build/.config"; then
        echo "BusyBox configuration must enable $option" >&2
        exit 1
    fi
done

"$make_path" -s -C "$busybox_stage" O="$busybox_build" ARCH=mips \
    CROSS_COMPILE="$cross_compile" -j"$jobs" busybox
install -m 0755 "$busybox_build/busybox" "$stage_busybox_elf"

if "$cross_readelf" -l "$stage_busybox_elf" | grep -q INTERP; then
    echo "BusyBox is still dynamically linked" >&2
    exit 1
fi
if "$cross_readelf" -A "$stage_busybox_elf" | grep -q 'Hard float'; then
    if [ ! -f "$kernel_config" ] ||
       ! grep -qx 'CONFIG_MIPS_FP_SUPPORT=y' "$kernel_config"; then
        echo "hard-float BusyBox requires CONFIG_MIPS_FP_SUPPORT=y in $kernel_config" >&2
        echo "merge linux/emu.config, run olddefconfig, and rebuild the kernel" >&2
        exit 1
    fi
fi

# gen_init_cpio can create the initial console device without host root
# privileges and fixes all metadata through initramfs.list.  Its stdout archive
# interface also works with older kernel source trees that lack -o.
"$host_cc_path" -O2 "$linux_dir/usr/gen_init_cpio.c" -o "$stage_host_gen"
MIPSEL_EMU_INIT="$script_dir/init" \
MIPSEL_EMU_BUSYBOX="$stage_busybox_elf" \
    "$stage_host_gen" -t 0 "$script_dir/initramfs.list" > "$stage_archive_raw"
"$gzip_path" -9n -c "$stage_archive_raw" > "$stage_archive"
chmod 0644 "$stage_archive_raw" "$stage_archive"

# Publish only complete files.  stage_dir is inside output_dir, so each rename
# stays on one filesystem and is atomic for readers.
mv "$stage_host_gen" "$host_gen"
mv "$stage_archive_raw" "$archive_raw"
mv "$stage_busybox_elf" "$busybox_elf"
mv "$stage_archive" "$archive"

echo "static BusyBox: $busybox_elf"
echo "initramfs:      $archive"
