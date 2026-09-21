cmake_minimum_required(VERSION 3.22)

project(mipsel_emu VERSION 0.1.0 LANGUAGES C)

option(MIPSEL_EMU_IS_EMBEDDED_SYSTEM
       "Build the size-constrained firmware profile (CONFIG_IS_EMBEDDED_SYSTEM)"
       OFF)

set(_mipsel_emu_host_default ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Generic" OR MIPSEL_EMU_IS_EMBEDDED_SYSTEM)
    set(_mipsel_emu_host_default OFF)
endif()
option(MIPSEL_EMU_BUILD_HOST
       "Build the hosted command-line or Qt frontend"
       ${_mipsel_emu_host_default})
# ncurses is a POSIX-only dependency.  Windows uses the terminal-free host
# debugger below; the Qt/SDL desktop front-end remains usable out of the box.
set(_mipsel_emu_tui_default ON)
if(WIN32)
    set(_mipsel_emu_tui_default OFF)
endif()
option(MIPSEL_EMU_ENABLE_TUI "Build the ncursesw debugger interface"
       ${_mipsel_emu_tui_default})
option(MIPSEL_EMU_ENABLE_UART16550 "Build the 16550A MMIO UART" ON)
option(MIPSEL_EMU_ENABLE_ELF_LOADER "Build ELF image loading support" ON)
option(MIPSEL_EMU_ENABLE_INITRAMFS "Build initramfs loading support" ON)
option(MIPSEL_EMU_ENABLE_CONSOLE
       "Build the transport-neutral paused-target monitor" ON)
option(MIPSEL_EMU_ENABLE_SDL
       "Build optional SDL framebuffer output for the hosted frontend" OFF)
set(_mipsel_emu_framebuffer_default ON)
if(MIPSEL_EMU_IS_EMBEDDED_SYSTEM)
    set(_mipsel_emu_framebuffer_default OFF)
endif()
option(MIPSEL_EMU_ENABLE_FRAMEBUFFER
       "Build the RGB565 framebuffer MMIO device"
       ${_mipsel_emu_framebuffer_default})
set(_mipsel_emu_qt_gui_default ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    set(_mipsel_emu_qt_gui_default OFF)
endif()
option(MIPSEL_EMU_ENABLE_QT_GUI
       "Build the Qt6 desktop frontend (no arguments starts the GUI)"
       ${_mipsel_emu_qt_gui_default})
option(MIPSEL_EMU_BUILD_DISPATCH_BENCHMARK
       "Build the native instruction-dispatch throughput benchmark"
       OFF)

if(MIPSEL_EMU_IS_EMBEDDED_SYSTEM)
    set(MIPSEL_EMU_BUILD_HOST OFF CACHE BOOL
        "The embedded profile has no POSIX frontend" FORCE)
    set(MIPSEL_EMU_ENABLE_TUI OFF CACHE BOOL
        "The embedded profile has no ncurses frontend" FORCE)
    set(MIPSEL_EMU_ENABLE_CONSOLE OFF CACHE BOOL
        "The embedded profile has no text monitor" FORCE)
    set(MIPSEL_EMU_ENABLE_SDL OFF CACHE BOOL
        "The embedded profile has no SDL output" FORCE)
    set(MIPSEL_EMU_ENABLE_QT_GUI OFF CACHE BOOL
        "The embedded profile has no Qt frontend" FORCE)
    set(MIPSEL_EMU_ENABLE_FRAMEBUFFER OFF CACHE BOOL
        "The embedded profile has no framebuffer MMIO" FORCE)
endif()

# Keep the preprocessor spelling requested by the firmware integration in
# sync with the CMake profile used to generate the installed config header.
set(CONFIG_IS_EMBEDDED_SYSTEM ${MIPSEL_EMU_IS_EMBEDDED_SYSTEM})
set(MIPSEL_EMU_USER_CONFIG_HEADER "" CACHE STRING
    "Board configuration header included by config.h")
set(MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR "" CACHE PATH
    "Include directory containing MIPSEL_EMU_USER_CONFIG_HEADER")

if(MIPSEL_EMU_BUILD_HOST AND NOT MIPSEL_EMU_ENABLE_ELF_LOADER)
    message(FATAL_ERROR
        "The host frontend requires MIPSEL_EMU_ENABLE_ELF_LOADER=ON")
endif()

if(MIPSEL_EMU_BUILD_HOST AND MIPSEL_EMU_ENABLE_QT_GUI AND
   NOT MIPSEL_EMU_ENABLE_CONSOLE)
    message(FATAL_ERROR
        "The Qt frontend requires MIPSEL_EMU_ENABLE_CONSOLE=ON for its Monitor tab")
endif()

set(MIPSEL_EMU_QT_ROOT "" CACHE PATH
    "Qt installation root used for automatic Windows Qt6 discovery")
set(MIPSEL_EMU_SDL2_ROOT "" CACHE PATH
    "SDL2 development package root used for automatic discovery")
set(MIPSEL_EMU_RESOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../res" CACHE PATH
    "Optional guest image directory copied beside the hosted executable")

if(MIPSEL_EMU_BUILD_HOST AND MIPSEL_EMU_ENABLE_QT_GUI)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    if(WIN32 AND (NOT Qt6_DIR OR Qt6_DIR MATCHES "-NOTFOUND$" OR
                  NOT EXISTS "${Qt6_DIR}"))
        # Preserve caller-provided prefixes, but also inspect the usual Qt
        # installation root.  This supports both CMAKE_PREFIX_PATH=D:/QtLib
        # (or D:/Qt) and a specific kit prefix without baking a machine path.
        if(NOT MIPSEL_EMU_QT_ROOT)
            # Qt installations commonly live in either the current Qt
            # Maintenance Tool prefix or the older D:/Qt layout.  Prefer the
            # former while retaining compatibility with existing machines.
            foreach(_mipsel_emu_default_qt_root IN ITEMS "D:/QtLib" "D:/Qt")
                if(EXISTS "${_mipsel_emu_default_qt_root}")
                    set(MIPSEL_EMU_QT_ROOT "${_mipsel_emu_default_qt_root}")
                    break()
                endif()
            endforeach()
        endif()
        if(MIPSEL_EMU_QT_ROOT)
            set(_mipsel_emu_qt_prefixes)
            if(EXISTS
               "${MIPSEL_EMU_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
                list(APPEND _mipsel_emu_qt_prefixes "${MIPSEL_EMU_QT_ROOT}")
            else()
                file(GLOB _mipsel_emu_qt_kits LIST_DIRECTORIES true
                     "${MIPSEL_EMU_QT_ROOT}/6.*/*")
                list(SORT _mipsel_emu_qt_kits COMPARE NATURAL ORDER DESCENDING)
                foreach(_mipsel_emu_qt_kit IN LISTS _mipsel_emu_qt_kits)
                    if(NOT IS_DIRECTORY "${_mipsel_emu_qt_kit}")
                        continue()
                    endif()
                    if(MSVC)
                        if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
                            if("${_mipsel_emu_qt_kit}" MATCHES
                               "[/\\\\]msvc[0-9]+_arm64$")
                                set(_mipsel_emu_qt_kit_matches TRUE)
                            else()
                                set(_mipsel_emu_qt_kit_matches FALSE)
                            endif()
                        else()
                            if("${_mipsel_emu_qt_kit}" MATCHES
                               "[/\\\\]msvc[0-9]+_64$")
                                set(_mipsel_emu_qt_kit_matches TRUE)
                            else()
                                set(_mipsel_emu_qt_kit_matches FALSE)
                            endif()
                        endif()
                    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
                        if("${_mipsel_emu_qt_kit}" MATCHES
                           "[/\\\\](mingw_64|llvm-mingw_64)$")
                            set(_mipsel_emu_qt_kit_matches TRUE)
                        else()
                            set(_mipsel_emu_qt_kit_matches FALSE)
                        endif()
                    else()
                        set(_mipsel_emu_qt_kit_matches TRUE)
                    endif()
                    if(NOT _mipsel_emu_qt_kit_matches)
                        continue()
                    endif()
                    if(EXISTS
                       "${_mipsel_emu_qt_kit}/lib/cmake/Qt6/Qt6Config.cmake")
                        list(APPEND _mipsel_emu_qt_prefixes
                             "${_mipsel_emu_qt_kit}")
                        break()
                    endif()
                endforeach()
            endif()
            if(_mipsel_emu_qt_prefixes)
                list(PREPEND CMAKE_PREFIX_PATH ${_mipsel_emu_qt_prefixes})
                message(STATUS "Using Qt6 from ${_mipsel_emu_qt_prefixes}")
            else()
                message(STATUS
                    "No Qt6Config.cmake found below ${MIPSEL_EMU_QT_ROOT}; "
                    "install the Qt6 Widgets development component or set Qt6_DIR")
            endif()
        endif()
    endif()
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# GCC extensions remain enabled for the firmware-oriented source set.  The
# switch dispatcher itself stays compatible with the MSVC compiler shim.
set(CMAKE_C_EXTENSIONS ON)

set(MIPSEL_EMU_CORE_SOURCES
    emu.c
    exception.c
    registers.c
    platform.c
    image_loader.c
)

if(NOT MIPSEL_EMU_IS_EMBEDDED_SYSTEM)
    list(APPEND MIPSEL_EMU_CORE_SOURCES observer.c)
endif()

if(MIPSEL_EMU_ENABLE_CONSOLE)
    list(APPEND MIPSEL_EMU_CORE_SOURCES console.c disasm.c format.c)
endif()

if(MIPSEL_EMU_ENABLE_UART16550)
    list(APPEND MIPSEL_EMU_CORE_SOURCES uart16550.c)
endif()

set(MIPSEL_EMU_PUBLIC_HEADERS
    compiler.h
    emu.h
    exception.h
    image_loader.h
    mipsel_emu.h
    observer.h
    platform.h
    registers.h
    uart16550.h
)

if(MIPSEL_EMU_ENABLE_CONSOLE)
    list(APPEND MIPSEL_EMU_PUBLIC_HEADERS console.h disasm.h)
endif()

# The source-tree config keeps desktop-friendly defaults for direct builds.
# An installed archive instead needs a header locked to the features that are
# actually present, otherwise (for example) a console-off library advertises
# declarations for symbols it does not contain.
set(MIPSEL_EMU_GENERATED_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include/mipsel_emu")
file(MAKE_DIRECTORY "${MIPSEL_EMU_GENERATED_INCLUDE_DIR}")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config.h"
     _mipsel_emu_installed_config)
set(_mipsel_emu_feature_config "")
set(_mipsel_emu_feature_lock_config "")
foreach(_mipsel_emu_feature
        MIPSEL_EMU_ENABLE_UART16550
        MIPSEL_EMU_ENABLE_ELF_LOADER
        MIPSEL_EMU_ENABLE_INITRAMFS
        MIPSEL_EMU_ENABLE_CONSOLE
        MIPSEL_EMU_ENABLE_FRAMEBUFFER
        CONFIG_IS_EMBEDDED_SYSTEM)
    if(${_mipsel_emu_feature})
        set(_mipsel_emu_feature_value 1)
    else()
        set(_mipsel_emu_feature_value 0)
    endif()
    string(APPEND _mipsel_emu_feature_config
        "#if defined(${_mipsel_emu_feature}) && "
        "${_mipsel_emu_feature} != ${_mipsel_emu_feature_value}\n"
        "#error \"${_mipsel_emu_feature} does not match the installed library\"\n"
        "#endif\n"
        "#undef ${_mipsel_emu_feature}\n"
        "#define ${_mipsel_emu_feature} ${_mipsel_emu_feature_value}\n")
    string(APPEND _mipsel_emu_feature_lock_config
        "#if ${_mipsel_emu_feature} != ${_mipsel_emu_feature_value}\n"
        "#error \"${_mipsel_emu_feature} does not match the installed library\"\n"
        "#endif\n")
endforeach()

# Resolve the numeric configuration through the selected compiler and board
# header.  These values affect the archive's ABI or platform behavior, so the
# installed header checks that consumers cannot silently compile against a
# different configuration.
set(_mipsel_emu_locked_macros
    MIPSEL_EMU_RAM_SIZE
    MIPSEL_EMU_CPU_CLOCK_HZ
    MIPSEL_EMU_CP0_COUNT_DIVIDER
    MIPSEL_EMU_UART_MMIO_BASE
    MIPSEL_EMU_UART_CLOCK_HZ
    MIPSEL_EMU_UART_IRQ_LINE
    MIPSEL_EMU_UART_RX_FIFO_SIZE
    MIPSEL_EMU_UART_TX_FIFO_SIZE
    MIPSEL_EMU_FB_MMIO_BASE
    MIPSEL_EMU_FB_WIDTH
    MIPSEL_EMU_FB_HEIGHT
    MIPSEL_EMU_FB_STRIDE_BYTES
    MIPSEL_EMU_IMAGE_CHUNK_SIZE
    MIPSEL_EMU_CONSOLE_LINE_SIZE
    MIPSEL_EMU_CONSOLE_OUTPUT_SIZE
    MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS
    MIPSEL_EMU_DTB_PHYSICAL_ADDRESS
    MIPSEL_EMU_DTB_RESERVED_SIZE
    MIPSEL_EMU_DTB_VIRTUAL_ADDRESS
    MIPSEL_EMU_INITRAMFS_ALIGNMENT
)
set(_mipsel_emu_config_probe
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mipsel_emu_config_probe.c")
file(WRITE "${_mipsel_emu_config_probe}" "#include \"config.h\"\n")
set(_mipsel_emu_probe_command "${CMAKE_C_COMPILER}")
if(CMAKE_C_COMPILER_ARG1)
    separate_arguments(_mipsel_emu_compiler_arg1 NATIVE_COMMAND
                       "${CMAKE_C_COMPILER_ARG1}")
    list(APPEND _mipsel_emu_probe_command ${_mipsel_emu_compiler_arg1})
endif()
if(NOT MSVC AND CMAKE_C_COMPILER_TARGET)
    list(APPEND _mipsel_emu_probe_command
         "--target=${CMAKE_C_COMPILER_TARGET}")
endif()
if(NOT MSVC AND CMAKE_SYSROOT_COMPILE)
    list(APPEND _mipsel_emu_probe_command
         "--sysroot=${CMAKE_SYSROOT_COMPILE}")
elseif(NOT MSVC AND CMAKE_SYSROOT)
    list(APPEND _mipsel_emu_probe_command "--sysroot=${CMAKE_SYSROOT}")
endif()
foreach(_mipsel_emu_feature
        MIPSEL_EMU_ENABLE_UART16550
        MIPSEL_EMU_ENABLE_ELF_LOADER
        MIPSEL_EMU_ENABLE_INITRAMFS
        MIPSEL_EMU_ENABLE_CONSOLE
        MIPSEL_EMU_ENABLE_FRAMEBUFFER
        CONFIG_IS_EMBEDDED_SYSTEM)
    if(${_mipsel_emu_feature})
        set(_mipsel_emu_feature_value 1)
    else()
        set(_mipsel_emu_feature_value 0)
    endif()
    if(MSVC)
        list(APPEND _mipsel_emu_probe_command
             "/D${_mipsel_emu_feature}=${_mipsel_emu_feature_value}")
    else()
        list(APPEND _mipsel_emu_probe_command
             "-D${_mipsel_emu_feature}=${_mipsel_emu_feature_value}")
    endif()
    file(APPEND "${_mipsel_emu_config_probe}"
         "#if ${_mipsel_emu_feature} != ${_mipsel_emu_feature_value}\n"
         "#error \"board config overrides ${_mipsel_emu_feature}\"\n"
         "#endif\n")
endforeach()
if(MIPSEL_EMU_USER_CONFIG_HEADER)
    if(MSVC)
        list(APPEND _mipsel_emu_probe_command
             "/DMIPSEL_EMU_USER_CONFIG_HEADER=\"${MIPSEL_EMU_USER_CONFIG_HEADER}\"")
    else()
        list(APPEND _mipsel_emu_probe_command
             "-DMIPSEL_EMU_USER_CONFIG_HEADER=\"${MIPSEL_EMU_USER_CONFIG_HEADER}\"")
    endif()
endif()
if(MSVC)
    list(APPEND _mipsel_emu_probe_command
         "/I${CMAKE_CURRENT_SOURCE_DIR}")
else()
    list(APPEND _mipsel_emu_probe_command
         "-I${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR)
    if(MSVC)
        list(APPEND _mipsel_emu_probe_command
             "/I${MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR}")
    else()
        list(APPEND _mipsel_emu_probe_command
             "-I${MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR}")
    endif()
endif()

if(MSVC)
    # The compiler probe is executed directly by CMake, outside the Visual
    # Studio project environment.  Supply the CRT and Windows SDK include
    # roots explicitly so a fresh PowerShell (without vcvarsall.bat) works as
    # well as a Developer Command Prompt.
    get_filename_component(_mipsel_emu_msvc_bin_dir
        "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(_mipsel_emu_msvc_host_dir
        "${_mipsel_emu_msvc_bin_dir}" DIRECTORY)
    get_filename_component(_mipsel_emu_msvc_bin_root
        "${_mipsel_emu_msvc_host_dir}" DIRECTORY)
    get_filename_component(_mipsel_emu_msvc_root
        "${_mipsel_emu_msvc_bin_root}" DIRECTORY)
    if(EXISTS "${_mipsel_emu_msvc_root}/include")
        list(APPEND _mipsel_emu_probe_command
             "/I${_mipsel_emu_msvc_root}/include")
    endif()

    # KitsRoot10 is present for normal Visual Studio installations.  The
    # ProgramFiles fallback also covers portable SDK layouts.
    get_filename_component(_mipsel_emu_windows_kits_root
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
        ABSOLUTE)
    if(NOT EXISTS "${_mipsel_emu_windows_kits_root}")
        set(_mipsel_emu_windows_kits_root
            "$ENV{ProgramFiles}/Windows Kits/10")
    endif()
    file(GLOB _mipsel_emu_windows_kits_roots
         "${_mipsel_emu_windows_kits_root}/Include/10.*")
    list(SORT _mipsel_emu_windows_kits_roots COMPARE NATURAL ORDER DESCENDING)
    list(LENGTH _mipsel_emu_windows_kits_roots
         _mipsel_emu_windows_kits_count)
    if(_mipsel_emu_windows_kits_count GREATER 0)
        list(GET _mipsel_emu_windows_kits_roots 0 _mipsel_emu_windows_kit)
        foreach(_mipsel_emu_windows_kit_include ucrt shared um)
            if(EXISTS "${_mipsel_emu_windows_kit}/${_mipsel_emu_windows_kit_include}")
                list(APPEND _mipsel_emu_probe_command
                     "/I${_mipsel_emu_windows_kit}/${_mipsel_emu_windows_kit_include}")
            endif()
        endforeach()
    endif()
endif()
# Match CMake's compile-command precedence: target definitions/includes come
# before global/configuration flags, so a conflicting -D in CMAKE_C_FLAGS is
# detected by the feature checks appended to the probe source.
separate_arguments(_mipsel_emu_probe_c_flags NATIVE_COMMAND
                   "${CMAKE_C_FLAGS}")
list(APPEND _mipsel_emu_probe_command ${_mipsel_emu_probe_c_flags})
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _mipsel_emu_build_type_upper)
    separate_arguments(_mipsel_emu_probe_config_c_flags NATIVE_COMMAND
        "${CMAKE_C_FLAGS_${_mipsel_emu_build_type_upper}}")
    list(APPEND _mipsel_emu_probe_command
         ${_mipsel_emu_probe_config_c_flags})
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_C_EXTENSIONS)
        list(APPEND _mipsel_emu_probe_command
             "-std=gnu${CMAKE_C_STANDARD}")
    else()
        list(APPEND _mipsel_emu_probe_command
             "-std=c${CMAKE_C_STANDARD}")
    endif()
endif()
if(MSVC)
    # cl.exe uses /EP (preprocess to stdout) and /TC (treat the probe as C).
    # Do not use /P here: it writes a source-adjacent .i file and leaves the
    # configure tree dirty when CMake invokes the compiler directly.
    list(APPEND _mipsel_emu_probe_command /nologo /EP /TC)
else()
    list(APPEND _mipsel_emu_probe_command -E -P -x c)
endif()
foreach(_mipsel_emu_macro IN LISTS _mipsel_emu_locked_macros)
    file(APPEND "${_mipsel_emu_config_probe}"
         "MIPSEL_EMU_CONFIG_VALUE_${_mipsel_emu_macro} "
         "${_mipsel_emu_macro} MIPSEL_EMU_CONFIG_VALUE_END\n")
endforeach()
list(APPEND _mipsel_emu_probe_command "${_mipsel_emu_config_probe}")
execute_process(
    COMMAND ${_mipsel_emu_probe_command}
    RESULT_VARIABLE _mipsel_emu_probe_result
    OUTPUT_VARIABLE _mipsel_emu_probe_output
    ERROR_VARIABLE _mipsel_emu_probe_error
)
if(NOT _mipsel_emu_probe_result EQUAL 0)
    message(FATAL_ERROR
        "Cannot resolve the installed mipsel-emu configuration:\n"
        "${_mipsel_emu_probe_error}")
endif()

set(_mipsel_emu_numeric_config "")
set(_mipsel_emu_locked_config "")
foreach(_mipsel_emu_macro IN LISTS _mipsel_emu_locked_macros)
    string(REGEX MATCH
        "MIPSEL_EMU_CONFIG_VALUE_${_mipsel_emu_macro}[ \t]+[^\r\n]*MIPSEL_EMU_CONFIG_VALUE_END"
        _mipsel_emu_macro_line "${_mipsel_emu_probe_output}")
    if(NOT _mipsel_emu_macro_line)
        message(FATAL_ERROR
            "The compiler did not resolve ${_mipsel_emu_macro} from config.h")
    endif()
    string(REGEX REPLACE
        "^MIPSEL_EMU_CONFIG_VALUE_${_mipsel_emu_macro}[ \t]+" ""
        _mipsel_emu_macro_value "${_mipsel_emu_macro_line}")
    string(REGEX REPLACE
        "[ \t]+MIPSEL_EMU_CONFIG_VALUE_END$" ""
        _mipsel_emu_macro_value "${_mipsel_emu_macro_value}")
    string(STRIP "${_mipsel_emu_macro_value}"
           _mipsel_emu_macro_value)
    string(APPEND _mipsel_emu_numeric_config
        "#if defined(${_mipsel_emu_macro}) && "
        "((${_mipsel_emu_macro}) != (${_mipsel_emu_macro_value}))\n"
        "#error \"${_mipsel_emu_macro} does not match the installed library\"\n"
        "#endif\n"
        "#undef ${_mipsel_emu_macro}\n"
        "#define ${_mipsel_emu_macro} (${_mipsel_emu_macro_value})\n")
    string(APPEND _mipsel_emu_locked_config
        "#if (${_mipsel_emu_macro}) != (${_mipsel_emu_macro_value})\n"
        "#error \"${_mipsel_emu_macro} does not match the installed library\"\n"
        "#endif\n")
endforeach()
string(CONCAT _mipsel_emu_config_injection
    "#define MIPSEL_EMU_CONFIG_H\n\n"
    "/* Generated for the installed archive; do not edit. */\n"
    "${_mipsel_emu_feature_config}")
string(REPLACE
    "#define MIPSEL_EMU_CONFIG_H\n"
    "${_mipsel_emu_config_injection}"
    _mipsel_emu_installed_config
    "${_mipsel_emu_installed_config}")
string(CONCAT _mipsel_emu_board_config_replacement
    "/* The installed archive is self-contained and configuration-locked. */\n"
    "#ifdef MIPSEL_EMU_USER_CONFIG_HEADER\n"
    "#error \"MIPSEL_EMU_USER_CONFIG_HEADER cannot reconfigure an installed library\"\n"
    "#endif\n"
    "${_mipsel_emu_numeric_config}")
string(REPLACE
    "#ifdef MIPSEL_EMU_USER_CONFIG_HEADER\n#include MIPSEL_EMU_USER_CONFIG_HEADER\n#endif"
    "${_mipsel_emu_board_config_replacement}"
    _mipsel_emu_installed_config
    "${_mipsel_emu_installed_config}")
string(CONCAT _mipsel_emu_config_lock
    "/* Installed archive configuration lock. */\n"
    "${_mipsel_emu_feature_lock_config}"
    "${_mipsel_emu_locked_config}\n#endif\n")
string(REGEX REPLACE
    "#endif[ \t\r\n]*$"
    "${_mipsel_emu_config_lock}"
    _mipsel_emu_installed_config
    "${_mipsel_emu_installed_config}")
file(WRITE "${MIPSEL_EMU_GENERATED_INCLUDE_DIR}/config.h"
     "${_mipsel_emu_installed_config}")
unset(_mipsel_emu_feature)
unset(_mipsel_emu_feature_value)
unset(_mipsel_emu_feature_config)
unset(_mipsel_emu_feature_lock_config)
unset(_mipsel_emu_config_injection)
unset(_mipsel_emu_board_config_replacement)
unset(_mipsel_emu_config_lock)
unset(_mipsel_emu_config_probe)
unset(_mipsel_emu_probe_command)
unset(_mipsel_emu_compiler_arg1)
unset(_mipsel_emu_probe_c_flags)
unset(_mipsel_emu_build_type_upper)
unset(_mipsel_emu_probe_config_c_flags)
unset(_mipsel_emu_probe_result)
unset(_mipsel_emu_probe_output)
unset(_mipsel_emu_probe_error)
unset(_mipsel_emu_msvc_bin_dir)
unset(_mipsel_emu_msvc_host_dir)
unset(_mipsel_emu_msvc_bin_root)
unset(_mipsel_emu_msvc_root)
unset(_mipsel_emu_windows_kits_roots)
unset(_mipsel_emu_windows_kits_root)
unset(_mipsel_emu_candidate_kits)
unset(_mipsel_emu_windows_kits_count)
unset(_mipsel_emu_windows_kit)
unset(_mipsel_emu_windows_kit_include)
unset(_mipsel_emu_numeric_config)
unset(_mipsel_emu_locked_config)
unset(_mipsel_emu_locked_macros)
unset(_mipsel_emu_macro)
unset(_mipsel_emu_macro_line)
unset(_mipsel_emu_macro_value)
unset(_mipsel_emu_installed_config)

add_library(mipsel_emu_core STATIC ${MIPSEL_EMU_CORE_SOURCES})
add_library(mipsel-emu::core ALIAS mipsel_emu_core)
add_library(mipsel-emu::embedded ALIAS mipsel_emu_core)

set_target_properties(mipsel_emu_core PROPERTIES
    EXPORT_NAME core
    PUBLIC_HEADER "${MIPSEL_EMU_PUBLIC_HEADERS}"
)

target_include_directories(mipsel_emu_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include/mipsel_emu>"
)

if(MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR)
    target_include_directories(mipsel_emu_core PUBLIC
        "$<BUILD_INTERFACE:${MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR}>")
endif()

target_compile_options(mipsel_emu_core
    PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
)

target_compile_definitions(mipsel_emu_core
    PUBLIC
        MIPSEL_EMU_ENABLE_UART16550=$<BOOL:${MIPSEL_EMU_ENABLE_UART16550}>
        MIPSEL_EMU_ENABLE_ELF_LOADER=$<BOOL:${MIPSEL_EMU_ENABLE_ELF_LOADER}>
        MIPSEL_EMU_ENABLE_INITRAMFS=$<BOOL:${MIPSEL_EMU_ENABLE_INITRAMFS}>
        MIPSEL_EMU_ENABLE_CONSOLE=$<BOOL:${MIPSEL_EMU_ENABLE_CONSOLE}>
        MIPSEL_EMU_ENABLE_FRAMEBUFFER=$<BOOL:${MIPSEL_EMU_ENABLE_FRAMEBUFFER}>
        CONFIG_IS_EMBEDDED_SYSTEM=$<BOOL:${MIPSEL_EMU_IS_EMBEDDED_SYSTEM}>
)

if(MIPSEL_EMU_BUILD_DISPATCH_BENCHMARK)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "MIPSEL_EMU_BUILD_DISPATCH_BENCHMARK requires a native build")
    endif()

    add_executable(mipsel_emu_dispatch_benchmark
        tests/dispatch_benchmark.c
    )
    target_link_libraries(mipsel_emu_dispatch_benchmark PRIVATE
        mipsel-emu::core
    )
    target_compile_options(mipsel_emu_dispatch_benchmark
        PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
    )
    target_compile_definitions(mipsel_emu_dispatch_benchmark PRIVATE
        MIPSEL_EMU_BENCHMARK_DISPATCH_MODE="force-inlined-switch"
        MIPSEL_EMU_BENCHMARK_COMPILER_ID="${CMAKE_C_COMPILER_ID}"
        MIPSEL_EMU_BENCHMARK_COMPILER_VERSION="${CMAKE_C_COMPILER_VERSION}"
        MIPSEL_EMU_BENCHMARK_BUILD_CONFIG="$<CONFIG>"
    )
endif()

if(MIPSEL_EMU_USER_CONFIG_HEADER)
    target_compile_definitions(mipsel_emu_core PUBLIC
        "$<BUILD_INTERFACE:MIPSEL_EMU_USER_CONFIG_HEADER=\"${MIPSEL_EMU_USER_CONFIG_HEADER}\">")
endif()

if(MIPSEL_EMU_BUILD_HOST)
    # Keep one executable entry point for compatibility: with no arguments it
    # opens the Qt monitor, while arguments are still handled by the CLI.
    # Windows selects a terminal-free debugger backend below; POSIX builds
    # retain the full ncurses/debugger implementation.
    if(WIN32 AND MIPSEL_EMU_ENABLE_TUI)
        message(FATAL_ERROR
            "MIPSEL_EMU_ENABLE_TUI is not supported on Windows; "
            "use the Qt GUI or the headless CLI debugger")
    endif()

    if(WIN32)
        set(_mipsel_emu_debugger_source debugger_headless.c)
    else()
        set(_mipsel_emu_debugger_source debugger.c)
    endif()
    add_library(mipsel_emu_host STATIC
        ${_mipsel_emu_debugger_source}
        runloop.c
    )
    add_library(mipsel-emu::host ALIAS mipsel_emu_host)
    target_include_directories(mipsel_emu_host
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(mipsel_emu_host
        PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
    )
    if(MSVC)
        target_compile_definitions(mipsel_emu_host
            PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_link_libraries(mipsel_emu_host PUBLIC mipsel-emu::core)

    if(MIPSEL_EMU_ENABLE_TUI)
        set(CURSES_NEED_NCURSES TRUE)
        set(CURSES_NEED_WIDE TRUE)
        find_package(Curses REQUIRED)

        if(NOT TARGET Curses::Curses)
            add_library(Curses::Curses INTERFACE IMPORTED)
            set_target_properties(Curses::Curses PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${CURSES_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${CURSES_LIBRARIES}"
                INTERFACE_COMPILE_OPTIONS "${CURSES_CFLAGS}"
            )
        endif()

        target_compile_definitions(mipsel_emu_host
            PUBLIC MIPSEL_EMU_HAVE_CURSES=1
        )
        target_link_libraries(mipsel_emu_host PUBLIC Curses::Curses)
    endif()

    add_executable(mipsel-emu loader.c)
    if(MIPSEL_EMU_ENABLE_QT_GUI)
        target_sources(mipsel-emu PRIVATE gui_main.cpp)
        target_link_libraries(mipsel-emu PRIVATE Qt6::Widgets)
    else()
        target_sources(mipsel-emu PRIVATE cli_main.c)
    endif()
    target_link_libraries(mipsel-emu PRIVATE mipsel-emu::host)

    target_compile_options(mipsel-emu
        PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
    )
    if(MSVC)
        target_compile_definitions(mipsel-emu
            PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    # Qt always has its native framebuffer tab.  SDL is optional for Qt, but
    # remains required when the CLI --sdl option is explicitly enabled.
    set(_mipsel_emu_sdl_requested OFF)
    if(MIPSEL_EMU_ENABLE_SDL)
        set(_mipsel_emu_sdl_requested ON)
    elseif(MIPSEL_EMU_ENABLE_QT_GUI)
        set(_mipsel_emu_sdl_requested OFF)
    endif()
    if(MIPSEL_EMU_ENABLE_SDL OR MIPSEL_EMU_ENABLE_QT_GUI)
        if(WIN32 AND (NOT SDL2_DIR OR SDL2_DIR MATCHES "-NOTFOUND$" OR
                      NOT EXISTS "${SDL2_DIR}"))
            if(NOT MIPSEL_EMU_SDL2_ROOT)
                set(_mipsel_emu_local_sdl_root
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/sdl-player/SDL2-2.30.4")
                if(EXISTS "${_mipsel_emu_local_sdl_root}/cmake/sdl2-config.cmake")
                    set(MIPSEL_EMU_SDL2_ROOT "${_mipsel_emu_local_sdl_root}")
                endif()
            endif()
            if(MIPSEL_EMU_SDL2_ROOT AND
               EXISTS "${MIPSEL_EMU_SDL2_ROOT}/cmake/sdl2-config.cmake")
                set(SDL2_DIR "${MIPSEL_EMU_SDL2_ROOT}/cmake")
            endif()
        endif()
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2)
            set(MIPSEL_EMU_SDL_TARGET SDL2::SDL2)
        elseif(TARGET SDL2::SDL2-static)
            set(MIPSEL_EMU_SDL_TARGET SDL2::SDL2-static)
        else()
            if(_mipsel_emu_sdl_requested)
                find_package(SDL2 MODULE REQUIRED)
            else()
                find_package(SDL2 MODULE QUIET)
            endif()
            if(SDL2_FOUND)
                add_library(mipsel_emu_sdl_dependency INTERFACE)
                target_include_directories(mipsel_emu_sdl_dependency SYSTEM INTERFACE
                    ${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
                target_link_libraries(mipsel_emu_sdl_dependency INTERFACE
                    ${SDL2_LIBRARIES} ${SDL2_LIBRARY})
                set(MIPSEL_EMU_SDL_TARGET mipsel_emu_sdl_dependency)
            endif()
        endif()
        if(MIPSEL_EMU_SDL_TARGET)
            target_sources(mipsel-emu PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/sdl-player/ui/ui_surface.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/sdl-player/platform/sdl/sdl_display.c")
            target_include_directories(mipsel-emu PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/sdl-player")
            if(MIPSEL_EMU_ENABLE_QT_GUI)
                target_compile_definitions(mipsel-emu PRIVATE
                    MIPSEL_EMU_QT_HAVE_SDL=1 PIXEL_FORMAT_RGB565)
                if(MIPSEL_EMU_ENABLE_SDL)
                    target_compile_definitions(mipsel-emu PRIVATE
                        MIPSEL_EMU_ENABLE_SDL=1)
                endif()
            else()
                target_compile_definitions(mipsel-emu PRIVATE
                    MIPSEL_EMU_ENABLE_SDL=1 PIXEL_FORMAT_RGB565)
            endif()
            target_link_libraries(mipsel-emu PRIVATE ${MIPSEL_EMU_SDL_TARGET})
        elseif(_mipsel_emu_sdl_requested)
            message(FATAL_ERROR "SDL2 is required when MIPSEL_EMU_ENABLE_SDL=ON")
        endif()
    endif()

    if(WIN32 AND TARGET SDL2::SDL2)
        add_custom_command(TARGET mipsel-emu POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:SDL2::SDL2>"
                    "$<TARGET_FILE_DIR:mipsel-emu>"
            COMMENT "Copying SDL2 runtime DLL")
    endif()

    if(WIN32 AND MIPSEL_EMU_ENABLE_QT_GUI)
        # windeployqt copies Qt6Core/Gui/Widgets and the platform plugin next
        # to the executable.  It is optional so a package can deploy Qt by a
        # separate installer when windeployqt is not present.
        set(_mipsel_emu_qt_bin_dir)
        if(TARGET Qt6::Core)
            foreach(_mipsel_emu_qt_config RELEASE RELWITHDEBINFO DEBUG)
                get_target_property(_mipsel_emu_qt_core_location Qt6::Core
                    "IMPORTED_LOCATION_${_mipsel_emu_qt_config}")
                if(_mipsel_emu_qt_core_location AND
                   NOT _mipsel_emu_qt_core_location MATCHES "-NOTFOUND$")
                    get_filename_component(_mipsel_emu_qt_bin_dir
                        "${_mipsel_emu_qt_core_location}" DIRECTORY)
                    break()
                endif()
            endforeach()
        endif()
        if(NOT _mipsel_emu_qt_bin_dir AND Qt6_DIR)
            get_filename_component(_mipsel_emu_qt_cmake_dir
                "${Qt6_DIR}" ABSOLUTE)
            get_filename_component(_mipsel_emu_qt_cmake_parent
                "${_mipsel_emu_qt_cmake_dir}" DIRECTORY)
            get_filename_component(_mipsel_emu_qt_lib_dir
                "${_mipsel_emu_qt_cmake_parent}" DIRECTORY)
            get_filename_component(_mipsel_emu_qt_prefix
                "${_mipsel_emu_qt_lib_dir}" DIRECTORY)
            set(_mipsel_emu_qt_bin_dir "${_mipsel_emu_qt_prefix}/bin")
        endif()
        find_program(MIPSEL_EMU_WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt.exe
            HINTS "${_mipsel_emu_qt_bin_dir}")
        if(MIPSEL_EMU_WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET mipsel-emu POST_BUILD
                COMMAND "${MIPSEL_EMU_WINDEPLOYQT_EXECUTABLE}"
                        --no-translations "$<TARGET_FILE:mipsel-emu>"
                WORKING_DIRECTORY "$<TARGET_FILE_DIR:mipsel-emu>"
                COMMENT "Deploying Qt runtime")
        else()
            message(STATUS
                "windeployqt not found; Qt runtime deployment is skipped")
        endif()
    endif()

    # Keep all local guest images next to the executable when the ignored
    # res/ directory is present.  Missing resources are allowed so the project
    # can still be built from a clean source archive.
    if(EXISTS "${MIPSEL_EMU_RESOURCE_DIR}")
        add_custom_command(TARGET mipsel-emu POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${MIPSEL_EMU_RESOURCE_DIR}"
                    "$<TARGET_FILE_DIR:mipsel-emu>/res"
            COMMENT "Copying guest resources from ${MIPSEL_EMU_RESOURCE_DIR}")
    endif()
    unset(_mipsel_emu_debugger_source)
    unset(_mipsel_emu_sdl_requested)
    unset(_mipsel_emu_local_sdl_root)
    unset(_mipsel_emu_qt_cmake_dir)
    unset(_mipsel_emu_qt_cmake_parent)
    unset(_mipsel_emu_qt_lib_dir)
    unset(_mipsel_emu_qt_prefix)
    unset(_mipsel_emu_qt_bin_dir)
    unset(_mipsel_emu_default_qt_root)
    unset(MIPSEL_EMU_SDL_TARGET)
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    if(TARGET mipsel_emu_dispatch_benchmark)
        add_test(NAME dispatch_benchmark_smoke
                 COMMAND mipsel_emu_dispatch_benchmark 10003 1 1003)
    endif()

    add_executable(exception_test tests/exception_test.c)
    target_link_libraries(exception_test PRIVATE mipsel-emu::core)
    add_test(NAME exception_test COMMAND exception_test)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/disasm_test.c")
        add_executable(disasm_test tests/disasm_test.c)
        target_link_libraries(disasm_test PRIVATE mipsel-emu::core)
        add_test(NAME disasm_test COMMAND disasm_test)
    endif()

    if(MIPSEL_EMU_ENABLE_CONSOLE AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/console_test.c")
        add_executable(console_test tests/console_test.c)
        target_link_libraries(console_test PRIVATE mipsel-emu::core)
        add_test(NAME console_test COMMAND console_test)
    endif()

    if(MIPSEL_EMU_ENABLE_UART16550 AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/uart16550_test.c")
        add_executable(uart16550_test tests/uart16550_test.c)
        target_link_libraries(uart16550_test PRIVATE mipsel-emu::core)
        add_test(NAME uart16550_test COMMAND uart16550_test)
    endif()

    if(MIPSEL_EMU_ENABLE_UART16550 AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/platform_uart_test.c")
        add_executable(platform_uart_test tests/platform_uart_test.c)
        target_link_libraries(platform_uart_test PRIVATE mipsel-emu::core)
        add_test(NAME platform_uart_test COMMAND platform_uart_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/platform_framebuffer_test.c")
        add_executable(platform_framebuffer_test
            tests/platform_framebuffer_test.c)
        target_link_libraries(platform_framebuffer_test PRIVATE mipsel-emu::core)
        add_test(NAME platform_framebuffer_test COMMAND platform_framebuffer_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/platform_memory_test.c")
        add_executable(platform_memory_test tests/platform_memory_test.c)
        target_link_libraries(platform_memory_test PRIVATE mipsel-emu::core)
        add_test(NAME platform_memory_test COMMAND platform_memory_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/timer_interrupt_test.c")
        add_executable(timer_interrupt_test tests/timer_interrupt_test.c)
        target_link_libraries(timer_interrupt_test PRIVATE mipsel-emu::core)
        add_test(NAME timer_interrupt_test COMMAND timer_interrupt_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/instruction_test.c")
        add_executable(instruction_test tests/instruction_test.c)
        target_link_libraries(instruction_test PRIVATE mipsel-emu::core)
        add_test(NAME instruction_test COMMAND instruction_test)
    endif()

    if(MIPSEL_EMU_ENABLE_ELF_LOADER AND MIPSEL_EMU_ENABLE_INITRAMFS AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/image_loader_test.c")
        add_executable(image_loader_test tests/image_loader_test.c)
        target_link_libraries(image_loader_test PRIVATE mipsel-emu::core)
        add_test(NAME image_loader_test COMMAND image_loader_test)
    endif()
endif()

install(TARGETS mipsel_emu_core
    EXPORT mipsel_emuTargets
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/mipsel_emu
)
install(FILES "${MIPSEL_EMU_GENERATED_INCLUDE_DIR}/config.h"
    DESTINATION include/mipsel_emu
)
install(EXPORT mipsel_emuTargets
    FILE mipsel_emuTargets.cmake
    NAMESPACE mipsel-emu::
    DESTINATION lib/cmake/mipsel_emu
)
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mipsel_emuConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mipsel_emuConfig.cmake"
    INSTALL_DESTINATION lib/cmake/mipsel_emu
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mipsel_emuConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mipsel_emuConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mipsel_emuConfigVersion.cmake"
    DESTINATION lib/cmake/mipsel_emu
)

# The DTB is a guest artifact, so do not search host tools in a cross build.
if(MIPSEL_EMU_BUILD_HOST)
    find_program(DTC_EXECUTABLE NAMES dtc)
    if(DTC_EXECUTABLE)
        set(MIPSEL_EMU_DTB "${CMAKE_CURRENT_BINARY_DIR}/mipsel-emu.dtb")
        add_custom_command(
            OUTPUT "${MIPSEL_EMU_DTB}"
            COMMAND "${DTC_EXECUTABLE}" -I dts -O dtb
                    -o "${MIPSEL_EMU_DTB}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/linux/mipsel-emu.dts"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linux/mipsel-emu.dts"
            VERBATIM
            COMMENT "Compiling the mipsel-emu guest device tree"
        )
        add_custom_target(mipsel-emu-dtb DEPENDS "${MIPSEL_EMU_DTB}")
    else()
        message(STATUS
            "dtc not found; the optional mipsel-emu-dtb target is disabled")
    endif()
endif()
