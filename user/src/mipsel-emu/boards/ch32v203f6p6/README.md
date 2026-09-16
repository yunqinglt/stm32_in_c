# CH32V203F6P6 host profile

This board profile builds the MIPS emulator as an RV32IMAC bare-metal archive:

```sh
cmake -S user/src/mipsel-emu -B build/mipsel-emu-ch32 \
  -DCMAKE_TOOLCHAIN_FILE=user/src/mipsel-emu/cmake/riscv32-elf.cmake \
  -DMIPSEL_EMU_USER_CONFIG_HEADER=board_mipsel_emu_config.h \
  -DMIPSEL_EMU_USER_CONFIG_INCLUDE_DIR=user/src/mipsel-emu/boards/ch32v203f6p6 \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/mipsel-emu-ch32 --target mipsel_emu_core -j
riscv64-elf-size --format=berkeley \
  build/mipsel-emu-ch32/libmipsel_emu_core.a
```

The linker script, reset code, and peripheral drivers are deliberately not
part of this archive.  The final firmware should use the CH32 memory map
(`0x08000000`, 32 KiB Flash; `0x20000000`, 10 KiB SRAM), keep the emulator
state in internal SRAM, and install `platform_memory_configure()` callbacks
for the APS6404L PSRAM.  Kernel, DTB, and initramfs images can be read from
the W25Q64 through the `mipsel_image_t` callback without copying them into
internal SRAM.

`CONFIG_IS_EMBEDDED_SYSTEM` disables the Qt/SDL/TUI/Monitor and framebuffer
paths while retaining MIPS32 little-endian integer execution, CP0/TLB/MMU,
exceptions, timer/interrupt sampling, ELF/FDT/initramfs loading, and the
16550-compatible guest UART.
