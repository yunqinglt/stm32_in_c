cmake_minimum_required(VERSION 3.22)

# You should modify the image size in this file.
set(LINKER_SCRIPT   "${CMAKE_CURRENT_SOURCE_DIR}/user/src/cortex_m0/STM32F042C6T6/QEMU.ld")

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gnu-arm-none-eabi.cmake")

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_PROJECT_NAME asm_stm32)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)

message("Build type: " ${CMAKE_BUILD_TYPE})

add_executable(${CMAKE_PROJECT_NAME}
    user/src/main.c
    user/inc/main.h

    user/inc/compiler.h

    ${user_src}
)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)



file(GLOB user_src
    "${CMAKE_CURRENT_SOURCE_DIR}/user/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/user/src/cortex_m0/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/user/src/cortex_m0/*/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/user/lib/*/*.c"
#        "${CMAKE_CURRENT_SOURCE_DIR}/user/lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c"
)

target_sources(${PROJECT_NAME} PRIVATE ${user_src})

target_include_directories(asm_stm32 PRIVATE 
    user/inc
    user/src/cortex_m0
    user/src/cortex_m0/STM32F042C6T6
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex 
            ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.elf 
            ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMENT "Generating HEX file..."
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary 
            ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.elf 
            ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating BIN file..."
)