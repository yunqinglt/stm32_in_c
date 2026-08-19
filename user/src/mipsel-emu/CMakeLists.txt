cmake_minimum_required(VERSION 3.22)

project(mipsel_emu VERSION 0.1.0 LANGUAGES C)

option(MIPSEL_EMU_ENABLE_TUI "Build the ncursesw debugger interface" ON)

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
    uart16550.c
    debugger.c
    disasm.c
)

add_library(mipsel_emu_core STATIC ${MIPSEL_EMU_CORE_SOURCES})
add_library(mipsel-emu::core ALIAS mipsel_emu_core)

target_include_directories(mipsel_emu_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(mipsel_emu_core
    PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
)

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

    target_compile_definitions(mipsel_emu_core
        PUBLIC MIPSEL_EMU_HAVE_CURSES=1
    )
    target_link_libraries(mipsel_emu_core PUBLIC Curses::Curses)
endif()

add_executable(mipsel-emu loader.c)
target_link_libraries(mipsel-emu PRIVATE mipsel-emu::core)
target_compile_options(mipsel-emu
    PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
)

include(CTest)
if(BUILD_TESTING)
    # The exception unit test intentionally exercises exception.c without a VM
    # frontend. Supply a no-op observer so it remains independent of curses,
    # RAM and UART ownership.
    set(MIPSEL_EMU_EXCEPTION_TEST_OBSERVER
        "${CMAKE_CURRENT_BINARY_DIR}/exception_test_observer.c")
    file(GENERATE OUTPUT "${MIPSEL_EMU_EXCEPTION_TEST_OBSERVER}" CONTENT [=[
#include "debugger.h"
void debugger_exception(const Registers *state, uint32_t info,
                        uint8_t code, VectorClass vector_class)
{
    (void)state;
    (void)info;
    (void)code;
    (void)vector_class;
}
]=])

    add_executable(exception_test
        tests/exception_test.c
        exception.c
        "${MIPSEL_EMU_EXCEPTION_TEST_OBSERVER}"
    )
    target_include_directories(exception_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME exception_test COMMAND exception_test)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/disasm_test.c")
        add_executable(disasm_test tests/disasm_test.c)
        target_link_libraries(disasm_test PRIVATE mipsel-emu::core)
        add_test(NAME disasm_test COMMAND disasm_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/uart16550_test.c")
        add_executable(uart16550_test tests/uart16550_test.c)
        target_link_libraries(uart16550_test PRIVATE mipsel-emu::core)
        add_test(NAME uart16550_test COMMAND uart16550_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/platform_uart_test.c")
        add_executable(platform_uart_test tests/platform_uart_test.c)
        target_link_libraries(platform_uart_test PRIVATE mipsel-emu::core)
        add_test(NAME platform_uart_test COMMAND platform_uart_test)
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
endif()

# The DTB is a guest artifact, so keep dtc optional for normal host builds.
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
    message(STATUS "dtc not found; the optional mipsel-emu-dtb target is disabled")
endif()
