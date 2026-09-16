# stm32_in_c

一个以 C 为主的嵌入式实验仓库。它包含两条相互独立但可复用底层代码的工作线：
ARM Cortex-M 裸机工程，以及可在 PC 或嵌入式宿主上运行 MIPS32EL Linux 的模拟器。

## 目录

| 路径 | 内容 |
| --- | --- |
| `user/src/cortex_m0`、`user/src/cortex_m4` | Cortex-M 启动代码、内存图和芯片寄存器定义。 |
| `user/inc`、`user/src/main.c` | 根目录 ARM 裸机工程的公共头与入口。 |
| `user/src/mipsel-emu` | 独立 CMake 的 MIPS32 Release 2 little-endian 模拟器。 |
| `tools/sdl-player` | SDL2 RGB565 虚拟屏幕与 UI surface。 |
| `tools/lvgl-demo` | 可加入 MIPS initramfs 的 LVGL 示例源码。 |
| `docs/mipsel-emu-architecture.md` | 多核、设备总线、显示和 GPU 的演进设计记录。 |

`build/`、外部 Linux/BusyBox/LVGL 源码、交叉工具链、kernel、DTB、initramfs、trace
和可执行文件均为本地可再生产物，不进入版本库。

## ARM 裸机工程

根目录 CMake 当前默认使用 `cmake/gnu-arm-none-eabi.cmake` 与
`user/src/cortex_m0/STM32F042C6T6/QEMU.ld`。在安装 ARM GNU 工具链后：

```sh
cmake -S . -B build/arm -DCMAKE_BUILD_TYPE=Debug
cmake --build build/arm
```

这会生成 ELF、HEX 与 BIN。目标芯片或链接脚本发生变化时，需同时调整根目录
`CMakeLists.txt` 的 linker script、include 路径和 source list。

## MIPS32EL 模拟器

模拟器使用自己的 CMake 工程，不受根目录 ARM bare-metal toolchain 影响。它实现
MIPS32r2 little-endian、CP0、TLB/MMU、异常、分支延迟槽、LL/SC、Count/Compare 定时器、
16550 UART、ELF/FDT/initramfs loader；宿主可选择 CLI/TUI、Qt6 监视器和 SDL2 虚拟屏幕。

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mipsel-emu -j
ctest --test-dir build/mipsel-emu --output-on-failure
```

Qt6 图形前端需要 `qt6-base`；SDL 输出需要 `sdl2`；TUI 需要 `ncurses`；构建 guest DTB
需要 `dtc`。模拟器用法、TUI Monitor 命令、MMIO 地址、Linux 启动方式和镜像 loader API
见 [mipsel-emu README](user/src/mipsel-emu/README.md)。

## CH32V203F6P6 嵌入式宿主

CH32V203F6P6（QingKe V4C、RV32IMAC、32 KiB Flash、10 KiB SRAM）使用外部 APS6404L
8 MiB PSRAM 作为 guest RAM、W25Q64 保存 guest 镜像。RV32 profile 会定义
`CONFIG_IS_EMBEDDED_SYSTEM=1`，裁剪 Qt/SDL/TUI/Monitor/framebuffer/observer，但不裁剪
Linux 所需的 MIPS32EL、CP0、TLB/MMU 与异常路径：

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu-ch32 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/user/src/mipsel-emu/cmake/riscv32-elf.cmake" \
  -DMIPSEL_EMU_USER_CONFIG_HEADER=board_mipsel_emu_config.h \
  -DMIPSEL_EMU_USER_CONFIG_INCLUDE_DIR="$PWD/user/src/mipsel-emu/boards/ch32v203f6p6" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/mipsel-emu-ch32 --target mipsel_emu_core -j2
```

该目标仅生成可链接的模拟器 archive；CH32 startup、linker script、SPI/USB/UART 驱动和
PSRAM backend 仍应由板级 firmware 提供。详细的内存后端与镜像流式读取约定见
[CH32 profile README](user/src/mipsel-emu/boards/ch32v203f6p6/README.md)。

若本地已存在 MIPS musl 工具链、Linux 与 BusyBox 源码，可构建无图形 guest：

```sh
JOBS=2 user/src/mipsel-emu/linux/build-embedded-guest.sh
```

它生成 MIPS32EL `vmlinux`、8 MiB headless DTB、静态 BusyBox 和 gzip initramfs 到
`build/`，不会把产物加入 Git。

## 开发约定

- 使用 out-of-tree build；不要提交 `build/`、toolchain、下载源码或二进制镜像。
- 模拟器的公共配置均由 `config.h` 和可选板级 config header 控制；库和调用方必须使用
  相同配置。
- 对模拟器 ISA、设备总线或显示架构的重大调整，应先更新
  `docs/mipsel-emu-architecture.md`，并运行相应的 `ctest`。
