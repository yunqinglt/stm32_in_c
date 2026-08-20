cmake_minimum_required(VERSION 3.22)

project(mipsel_emu VERSION 0.1.0 LANGUAGES C)

set(_mipsel_emu_host_default ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    set(_mipsel_emu_host_default OFF)
endif()
option(MIPSEL_EMU_BUILD_HOST
       "Build the POSIX command-line frontend and debugger"
       ${_mipsel_emu_host_default})
option(MIPSEL_EMU_ENABLE_TUI "Build the ncursesw debugger interface" ON)
option(MIPSEL_EMU_ENABLE_UART16550 "Build the 16550A MMIO UART" ON)
option(MIPSEL_EMU_ENABLE_ELF_LOADER "Build ELF image loading support" ON)
option(MIPSEL_EMU_ENABLE_INITRAMFS "Build initramfs loading support" ON)
option(MIPSEL_EMU_ENABLE_CONSOLE
       "Build the transport-neutral paused-target monitor" ON)
set(MIPSEL_EMU_USER_CONFIG_HEADER "" CACHE STRING
    "Board configuration header included by config.h")
set(MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR "" CACHE PATH
    "Include directory containing MIPSEL_EMU_USER_CONFIG_HEADER")

if(MIPSEL_EMU_BUILD_HOST AND NOT MIPSEL_EMU_ENABLE_ELF_LOADER)
    message(FATAL_ERROR
        "The host frontend requires MIPSEL_EMU_ENABLE_ELF_LOADER=ON")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# The instruction tables use GNU range designators and GCC attributes.
set(CMAKE_C_EXTENSIONS ON)

set(MIPSEL_EMU_CORE_SOURCES
    emu.c
    instru.c
    op.c
    exception.c
    registers.c
    platform.c
    observer.c
    image_loader.c
    disasm.c
    format.c
)

if(MIPSEL_EMU_ENABLE_CONSOLE)
    list(APPEND MIPSEL_EMU_CORE_SOURCES console.c)
endif()

if(MIPSEL_EMU_ENABLE_UART16550)
    list(APPEND MIPSEL_EMU_CORE_SOURCES uart16550.c)
endif()

set(MIPSEL_EMU_PUBLIC_HEADERS
    compiler.h
    disasm.h
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
    list(APPEND MIPSEL_EMU_PUBLIC_HEADERS console.h)
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
        MIPSEL_EMU_ENABLE_CONSOLE)
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
    MIPSEL_EMU_UART_MMIO_BASE
    MIPSEL_EMU_UART_CLOCK_HZ
    MIPSEL_EMU_UART_IRQ_LINE
    MIPSEL_EMU_UART_RX_FIFO_SIZE
    MIPSEL_EMU_UART_TX_FIFO_SIZE
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
if(CMAKE_C_COMPILER_TARGET)
    list(APPEND _mipsel_emu_probe_command
         "--target=${CMAKE_C_COMPILER_TARGET}")
endif()
if(CMAKE_SYSROOT_COMPILE)
    list(APPEND _mipsel_emu_probe_command
         "--sysroot=${CMAKE_SYSROOT_COMPILE}")
elseif(CMAKE_SYSROOT)
    list(APPEND _mipsel_emu_probe_command "--sysroot=${CMAKE_SYSROOT}")
endif()
foreach(_mipsel_emu_feature
        MIPSEL_EMU_ENABLE_UART16550
        MIPSEL_EMU_ENABLE_ELF_LOADER
        MIPSEL_EMU_ENABLE_INITRAMFS
        MIPSEL_EMU_ENABLE_CONSOLE)
    if(${_mipsel_emu_feature})
        set(_mipsel_emu_feature_value 1)
    else()
        set(_mipsel_emu_feature_value 0)
    endif()
    list(APPEND _mipsel_emu_probe_command
         "-D${_mipsel_emu_feature}=${_mipsel_emu_feature_value}")
    file(APPEND "${_mipsel_emu_config_probe}"
         "#if ${_mipsel_emu_feature} != ${_mipsel_emu_feature_value}\n"
         "#error \"board config overrides ${_mipsel_emu_feature}\"\n"
         "#endif\n")
endforeach()
if(MIPSEL_EMU_USER_CONFIG_HEADER)
    list(APPEND _mipsel_emu_probe_command
         "-DMIPSEL_EMU_USER_CONFIG_HEADER=\"${MIPSEL_EMU_USER_CONFIG_HEADER}\"")
endif()
list(APPEND _mipsel_emu_probe_command
     "-I${CMAKE_CURRENT_SOURCE_DIR}")
if(MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR)
    list(APPEND _mipsel_emu_probe_command
         "-I${MIPSEL_EMU_USER_CONFIG_INCLUDE_DIR}")
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
list(APPEND _mipsel_emu_probe_command -E -P -x c)
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
)

if(MIPSEL_EMU_USER_CONFIG_HEADER)
    target_compile_definitions(mipsel_emu_core PUBLIC
        "$<BUILD_INTERFACE:MIPSEL_EMU_USER_CONFIG_HEADER=\"${MIPSEL_EMU_USER_CONFIG_HEADER}\">")
endif()

if(MIPSEL_EMU_BUILD_HOST)
    add_library(mipsel_emu_host STATIC
        debugger.c
        runloop.c
    )
    add_library(mipsel-emu::host ALIAS mipsel_emu_host)
    target_include_directories(mipsel_emu_host
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(mipsel_emu_host
        PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
    )
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
    target_link_libraries(mipsel-emu PRIVATE mipsel-emu::host)
    target_compile_options(mipsel-emu
        PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
    )
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
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
