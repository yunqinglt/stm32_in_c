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
