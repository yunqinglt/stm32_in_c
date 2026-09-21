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

本地 guest 镜像可以放在仓库根目录的 `res/` 中；该目录已加入 `.gitignore`，不会被
提交。Windows 主机构建时，CMake 会把其中存在的 guest 资源复制到可执行文件旁的
`res/`，因此可以直接用同一份资源启动调试器。

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

### Windows/MSVC 主机

Windows 使用 Visual Studio 的 MSVC x64 生成器；Windows 默认关闭 ncurses TUI，保留
模拟器核心、无 POSIX 依赖的 CLI 和 Qt6/SDL2 桌面前端。下面的命令以本机 Visual Studio
安装在 `E:/Microsoft Visual Studio`、Qt 安装在 `D:/QtLib` 为例（旧安装也可使用
`D:/Qt`）。`MIPSEL_EMU_QT_ROOT`
会扫描匹配 MSVC x64 的 Qt kit；SDL2 可直接使用仓库内的
`tools/sdl-player/SDL2-2.30.4`：

```powershell
cmake -S user/src/mipsel-emu -B build/mipsel-emu-win `
  -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_GENERATOR_INSTANCE="E:/Microsoft Visual Studio/18/Community" `
  -DMIPSEL_EMU_ENABLE_TUI=OFF `
  -DMIPSEL_EMU_ENABLE_QT_GUI=ON `
  -DMIPSEL_EMU_ENABLE_SDL=ON `
  -DMIPSEL_EMU_QT_ROOT="D:/QtLib" `
  -DMIPSEL_EMU_SDL2_ROOT="tools/sdl-player/SDL2-2.30.4"
cmake --build build/mipsel-emu-win --config Debug
ctest --test-dir build/mipsel-emu-win -C Debug --output-on-failure
```

Qt kit 必须包含 Qt6 Widgets 的开发文件（`lib/cmake/Qt6/Qt6Config.cmake`）和与 MSVC
架构匹配的二进制；构建后若找到 `windeployqt`，CMake 会部署 Qt DLL 和平台插件，SDL2
DLL 也会复制到 `Debug`/`Release` 目录。

`D:/QtLib` 的默认安装若只有 `6.11.2/mingw_64`，应使用该 kit 自带的
`D:/QtLib/Tools/mingw1310_64` 编译器；MSVC 生成器需要另外安装匹配的 `msvc*_64` Qt kit。

Qt 6.11.2 MinGW kit 的完整构建示例（同时启用 SDL 镜像）如下：

```powershell
cmake -S user/src/mipsel-emu -B build/mipsel-emu-win-qt-mingw `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER="D:/QtLib/Tools/mingw1310_64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="D:/QtLib/Tools/mingw1310_64/bin/g++.exe" `
  -DMIPSEL_EMU_ENABLE_TUI=OFF `
  -DMIPSEL_EMU_ENABLE_QT_GUI=ON `
  -DMIPSEL_EMU_ENABLE_SDL=ON `
  -DMIPSEL_EMU_QT_ROOT="D:/QtLib" `
  -DMIPSEL_EMU_SDL2_ROOT="tools/sdl-player/SDL2-2.30.4"
cmake --build build/mipsel-emu-win-qt-mingw --parallel 4
ctest --test-dir build/mipsel-emu-win-qt-mingw --output-on-failure
```

若只验证核心或 CLI，可关闭 Qt/SDL：

```powershell
cmake -S user/src/mipsel-emu -B build/mipsel-emu-win-cli `
  -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_GENERATOR_INSTANCE="E:/Microsoft Visual Studio/18/Community" `
  -DMIPSEL_EMU_ENABLE_TUI=OFF -DMIPSEL_EMU_ENABLE_QT_GUI=OFF `
  -DMIPSEL_EMU_ENABLE_SDL=OFF
cmake --build build/mipsel-emu-win-cli --config Debug
ctest --test-dir build/mipsel-emu-win-cli -C Debug --output-on-failure
```

更完整的 Windows 路径、Qt/SDL 运行方式和 `res/` 资源约定见
[mipsel-emu Windows 构建说明](user/src/mipsel-emu/README.md)。

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
