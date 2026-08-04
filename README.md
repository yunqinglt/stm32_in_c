# stm32_in_c

This repository contains an STM32-focused embedded project written in C with assembly support. It includes startup code, memory definitions, and target-specific device headers for several ARM Cortex-M microcontrollers.

## Repository structure

- `CMakeLists.txt` - project build configuration
- `cmake/` - CMake toolchain and helper files
- `tools/` - project helper scripts
- `user/src/` - source code for supported platforms and emulation
- `user/inc/` - project headers and common definitions

## Supported targets

- STM32F042C6T6
- STM32F042C8T6
- STM32F405RGT6
- PY32E407V1ET7

## Build

Use CMake to configure and build the project in a separate `build` directory:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

## Notes

The repository excludes the following generated files and directories:

- `build/`
- `.iis/`
- `PY_LINK_V2.12.hex`

### MIPS32EL Release 2 Emulator(/user/src/mipsel-emu)
```C
// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    Features: 
    [*] single pipeline
    [/] basic mips32 release 2 behavior
    [ ] branch delay
    [ ] virtual mmio device
    [ ] ->   UART Console
    [ ] ->   Display
    [ ] Multi-Thread Application-Specific Extension
    [ ] ->   shadow register set
    [ ] virtual debug support
    [ ] ->   GDB

    [-] FPU

    So hard!!!!!!!!
    [-] 5-way pipeline simulation
*/
```