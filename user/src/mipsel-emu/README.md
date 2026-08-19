# MIPS32EL emulator

这是一个在宿主机上运行的 MIPS32 Release 2 小端模拟器。它与仓库根目录的
STM32 固件目标使用不同工具链：固件由 `arm-none-eabi-gcc` 交叉编译，模拟器必须
由宿主 C 编译器构建，因此本目录提供独立的 CMake 工程。

## 宿主构建

需要 CMake 3.22 以上和宿主 C11 编译器。TUI 默认启用，Linux 上还需要
`ncursesw` 开发包；Debian/Ubuntu 对应 `libncursesw5-dev`，Arch Linux 对应
`ncurses`。生成 guest DTB 另需 `dtc`（Debian/Ubuntu 包名
`device-tree-compiler`）。

从仓库根目录执行：

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mipsel-emu
ctest --test-dir build/mipsel-emu --output-on-failure
```

若宿主没有 ncursesw，可构建无 TUI 版本：

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu-headless \
  -DMIPSEL_EMU_ENABLE_TUI=OFF
cmake --build build/mipsel-emu-headless
```

## 设备树和 UHI 启动约定

设备树描述 16 MiB RAM，以及位于物理地址 `0x1f000900` 的 16550A UART。
UART 寄存器宽度为 32 位、间隔为 4 字节（`reg-io-width = 4`、
`reg-shift = 2`），输入时钟为 14.7456 MHz，连接到 CPU 硬件中断 4。

安装了 `dtc` 时，CMake 会提供 DTB 目标：

```sh
cmake --build build/mipsel-emu --target mipsel-emu-dtb
```

也可以直接生成：

```sh
dtc -I dts -O dtb \
  -o build/mipsel-emu/mipsel-emu.dtb \
  user/src/mipsel-emu/linux/mipsel-emu.dts
```

loader 将 DTB 复制到物理地址 `0x00010000`，并按 MIPS UHI 协议进入内核：

- `a0 = 0xfffffffe`（有符号值 `-2`，表示 UHI/FDT 启动）；
- `a1 = 0x80010000`（DTB 的 KSEG0 地址）；
- `a2 = a3 = 0`。

设备树的 `chosen` 节点使用 `earlycon` 和 `stdout-path`。这里特意使用不带显式
MMIO 参数的 `earlycon`，让内核从 DTB 获取 `reg-shift`、访问宽度和 UART 时钟。

## Linux 配置与构建

`linux/emu.config` 是针对现有 MIPS generic/UHI 配置的增量 fragment，打开
printk、TTY、8250 console、OF probing 和 DTB 命令行。以下命令保留已有的
`.config` 并合入 fragment：

```sh
repo_root=$PWD
linux_dir="$repo_root/linux-7.1.4"
cross_prefix="$repo_root/mips32el--musl--stable-2025.08-1/bin/mipsel-buildroot-linux-musl-"

"$linux_dir/scripts/kconfig/merge_config.sh" -m -O "$linux_dir" \
  "$linux_dir/.config" \
  "$repo_root/user/src/mipsel-emu/linux/emu.config"
make -C "$linux_dir" ARCH=mips CROSS_COMPILE="$cross_prefix" olddefconfig
make -C "$linux_dir" ARCH=mips CROSS_COMPILE="$cross_prefix" -j"$(nproc)" vmlinuz
```

全新配置可先执行 `make ... 32r2el_defconfig`，再合入 fragment。该 defconfig
启用的功能较多，最终应检查 ELF 的所有 `PT_LOAD` 段和 BSS 是否仍能放入
16 MiB guest RAM。

## 运行

CLI 形式为：

```text
mipsel-emu [--kernel FILE] [--dtb FILE] [--tui] [--run]
            [--trace FILE] [--max-steps N]
```

`--kernel` 也可以写成第一个位置参数，默认值为当前目录下的 `./vmlinuz`。
`--tui` 默认暂停在第一条指令；同时给出 `--run` 可立即连续执行。

从仓库根目录启动新构建的内核和 DTB：

```sh
build/mipsel-emu/mipsel-emu \
  --kernel linux-7.1.4/vmlinuz \
  --dtb build/mipsel-emu/mipsel-emu.dtb \
  --tui --run \
  --trace build/mipsel-emu/kernel.trace
```

不加 `--tui` 时使用 headless console；可配合 `--max-steps` 做有界的回归运行。
UART 输出在 TUI 模式进入 console window，headless 模式写入宿主 stdout，指令
与异常 trace 不应与 UART 字节流共用 stdout。headless 模式也会把 stdin 送入
UART；连接终端时使用原始输入，`Ctrl-] q` 退出模拟器，连续按两次 `Ctrl-]`
可向 guest 发送一个字面 `Ctrl-]`。TUI 中的 `r` 会清空 RAM、重新加载 ELF/DTB
并复位 CPU 和 UART。TUI 与 `--trace -` 不能共用终端，请指定独立 trace 文件。

## 当前范围

- 8250/16550 的基本寄存器、收发 FIFO 和 IRQ 已可用；接收 IRQ 当前在任意字节
  到达时立即置位，尚未模拟 FCR 阈值和字符超时的精确时序。
- `linux/emu.config` 只提供 console/UHI 所需增量配置，不包含 initramfs 或块设备
  rootfs；启动到 userspace 仍需另外提供 rootfs。
- CPU ISA、TLB 和平台设备仍在完善中。建议先用 `--max-steps` 做有界启动，并用
  TUI exception window 或 `--trace FILE` 定位尚未实现的 guest 行为。
