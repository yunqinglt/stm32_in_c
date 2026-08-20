# Dedicated toolchain for building the emulator's portable static library.
# Board startup code, linker scripts, specs files, and firmware link flags are
# deliberately outside this file and belong to the final firmware target.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX "arm-none-eabi" CACHE STRING
    "ARM bare-metal toolchain prefix, optionally including its absolute path")
set(MIPSEL_EMU_ARM_CPU "cortex-m0" CACHE STRING
    "ARM CPU passed to -mcpu when compiling the emulator library")
set(MIPSEL_EMU_ARM_FLOAT_ABI "soft" CACHE STRING
    "Floating-point ABI used by every object in the final firmware")
set(MIPSEL_EMU_ARM_FPU "" CACHE STRING
    "Optional -mfpu value, for example fpv4-sp-d16 on a Cortex-M4F")

set(CMAKE_C_COMPILER
    "${MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_ASM_COMPILER
    "${MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_AR
    "${MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX}-ar")
set(CMAKE_RANLIB
    "${MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX}-ranlib")
set(CMAKE_OBJCOPY
    "${MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX}-objcopy")
set(CMAKE_SIZE
    "${MIPSEL_EMU_ARM_TOOLCHAIN_PREFIX}-size")

set(_MIPSEL_EMU_ARM_FLAGS
    "-mcpu=${MIPSEL_EMU_ARM_CPU} -mthumb -mfloat-abi=${MIPSEL_EMU_ARM_FLOAT_ABI} -ffreestanding -ffunction-sections -fdata-sections")
if(MIPSEL_EMU_ARM_FPU)
    string(APPEND _MIPSEL_EMU_ARM_FLAGS " -mfpu=${MIPSEL_EMU_ARM_FPU}")
endif()

# _INIT variables are consumed once when CMake initializes a build tree.  This
# avoids the repeated flag appending caused by mutating CMAKE_C_FLAGS directly
# from a toolchain file that CMake may evaluate more than once.
set(CMAKE_C_FLAGS_INIT "${_MIPSEL_EMU_ARM_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${_MIPSEL_EMU_ARM_FLAGS}")

# This module toolchain intentionally produces only libmipsel_emu_core.a.
set(MIPSEL_EMU_BUILD_HOST OFF CACHE BOOL
    "Do not build POSIX frontend in the bare-metal configuration" FORCE)
set(BUILD_TESTING OFF CACHE BOOL
    "Host tests cannot run under a Generic bare-metal toolchain" FORCE)
