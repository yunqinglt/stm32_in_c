# RISC-V RV32IMAC bare-metal toolchain for the CH32V203 host firmware.
#
# The Arch Linux riscv64-elf GCC package includes an RV32IMAC multilib.  The
# final CH32 startup/linker code remains board-owned; this file only builds the
# portable emulator archive with the correct ABI and embedded feature profile.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX "riscv64-elf" CACHE STRING
    "RISC-V bare-metal toolchain prefix, optionally including an absolute path")
set(MIPSEL_EMU_RISCV_ARCH "rv32imac" CACHE STRING
    "RISC-V ISA used by the host MCU")
set(MIPSEL_EMU_RISCV_ABI "ilp32" CACHE STRING
    "RISC-V ABI used by the host MCU")

set(CMAKE_C_COMPILER "${MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_ASM_COMPILER "${MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_AR "${MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX}-ar")
set(CMAKE_RANLIB "${MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX}-ranlib")
set(CMAKE_OBJCOPY "${MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX}-objcopy")
set(CMAKE_SIZE "${MIPSEL_EMU_RISCV_TOOLCHAIN_PREFIX}-size")

set(_MIPSEL_EMU_RISCV_FLAGS
    "-march=${MIPSEL_EMU_RISCV_ARCH} -mabi=${MIPSEL_EMU_RISCV_ABI} -mno-relax -ffreestanding -fno-builtin -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_INIT "${_MIPSEL_EMU_RISCV_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${_MIPSEL_EMU_RISCV_FLAGS}")

set(MIPSEL_EMU_IS_EMBEDDED_SYSTEM ON CACHE BOOL
    "Build the size-constrained firmware profile" FORCE)
set(MIPSEL_EMU_BUILD_HOST OFF CACHE BOOL
    "Do not build the POSIX frontend for the MCU" FORCE)
set(MIPSEL_EMU_ENABLE_TUI OFF CACHE BOOL
    "Do not build ncurses for the MCU" FORCE)
set(MIPSEL_EMU_ENABLE_QT_GUI OFF CACHE BOOL
    "Do not build Qt for the MCU" FORCE)
set(MIPSEL_EMU_ENABLE_SDL OFF CACHE BOOL
    "Do not build SDL for the MCU" FORCE)
set(MIPSEL_EMU_ENABLE_CONSOLE OFF CACHE BOOL
    "Do not build the desktop monitor for the MCU" FORCE)
set(MIPSEL_EMU_ENABLE_FRAMEBUFFER OFF CACHE BOOL
    "Do not build framebuffer MMIO for the MCU" FORCE)
set(BUILD_TESTING OFF CACHE BOOL
    "Host tests cannot run under the bare-metal toolchain" FORCE)
