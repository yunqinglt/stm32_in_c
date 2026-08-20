#ifndef MIPSEL_EMU_CONFIG_H
#define MIPSEL_EMU_CONFIG_H

/*
 * Public build-time configuration for the portable emulator library.
 *
 * When building the library from source, a board may either define individual
 * MIPSEL_EMU_* macros on the compiler command line, or provide a header with:
 *
 *   -DMIPSEL_EMU_USER_CONFIG_HEADER=\"board_mipsel_emu_config.h\"
 *
 * Defaults intentionally describe the desktop reference platform and its
 * device tree.  Installed copies of this header are generated, self-contained,
 * and locked to the archive's build configuration.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef MIPSEL_EMU_USER_CONFIG_HEADER
#include MIPSEL_EMU_USER_CONFIG_HEADER
#endif

#ifndef MIPSEL_EMU_RAM_SIZE
#define MIPSEL_EMU_RAM_SIZE (16u * 1024u * 1024u)
#endif

#ifndef MIPSEL_EMU_ENABLE_UART16550
#define MIPSEL_EMU_ENABLE_UART16550 1
#endif

#ifndef MIPSEL_EMU_UART_MMIO_BASE
#define MIPSEL_EMU_UART_MMIO_BASE UINT32_C(0x1f000900)
#endif

#ifndef MIPSEL_EMU_UART_CLOCK_HZ
#define MIPSEL_EMU_UART_CLOCK_HZ 14745600u
#endif

#ifndef MIPSEL_EMU_UART_IRQ_LINE
#define MIPSEL_EMU_UART_IRQ_LINE 4u
#endif

#ifndef MIPSEL_EMU_UART_RX_FIFO_SIZE
#define MIPSEL_EMU_UART_RX_FIFO_SIZE 16u
#endif

#ifndef MIPSEL_EMU_UART_TX_FIFO_SIZE
#define MIPSEL_EMU_UART_TX_FIFO_SIZE 16u
#endif

/* Stack scratch used while copying images between flash and guest RAM. */
#ifndef MIPSEL_EMU_IMAGE_CHUNK_SIZE
#define MIPSEL_EMU_IMAGE_CHUNK_SIZE 256u
#endif

#ifndef MIPSEL_EMU_ENABLE_ELF_LOADER
#define MIPSEL_EMU_ENABLE_ELF_LOADER 1
#endif

#ifndef MIPSEL_EMU_ENABLE_INITRAMFS
#define MIPSEL_EMU_ENABLE_INITRAMFS 1
#endif

#ifndef MIPSEL_EMU_ENABLE_CONSOLE
#define MIPSEL_EMU_ENABLE_CONSOLE 1
#endif

/* Fixed storage used by the transport-neutral CDC/TUI command line. */
#ifndef MIPSEL_EMU_CONSOLE_LINE_SIZE
#define MIPSEL_EMU_CONSOLE_LINE_SIZE 160u
#endif

#ifndef MIPSEL_EMU_CONSOLE_OUTPUT_SIZE
#define MIPSEL_EMU_CONSOLE_OUTPUT_SIZE 192u
#endif

#ifndef MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS
#define MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS 16u
#endif

#ifndef MIPSEL_EMU_DTB_PHYSICAL_ADDRESS
#define MIPSEL_EMU_DTB_PHYSICAL_ADDRESS UINT32_C(0x00010000)
#endif

#ifndef MIPSEL_EMU_DTB_RESERVED_SIZE
#define MIPSEL_EMU_DTB_RESERVED_SIZE UINT32_C(0x00010000)
#endif

#ifndef MIPSEL_EMU_DTB_VIRTUAL_ADDRESS
#define MIPSEL_EMU_DTB_VIRTUAL_ADDRESS \
    (UINT32_C(0x80000000) | MIPSEL_EMU_DTB_PHYSICAL_ADDRESS)
#endif

#ifndef MIPSEL_EMU_INITRAMFS_ALIGNMENT
#define MIPSEL_EMU_INITRAMFS_ALIGNMENT 4096u
#endif

#if MIPSEL_EMU_RAM_SIZE == 0
#error "MIPSEL_EMU_RAM_SIZE must be greater than zero"
#endif

#if MIPSEL_EMU_RAM_SIZE > UINT32_MAX
#error "MIPSEL_EMU_RAM_SIZE must fit the 32-bit physical address API"
#endif

#if MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS == 0
#error "MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS must be greater than zero"
#endif

#if MIPSEL_EMU_UART_RX_FIFO_SIZE == 0 || MIPSEL_EMU_UART_TX_FIFO_SIZE == 0
#error "MIPSEL_EMU UART FIFO sizes must be greater than zero"
#endif

#if MIPSEL_EMU_IMAGE_CHUNK_SIZE == 0
#error "MIPSEL_EMU_IMAGE_CHUNK_SIZE must be greater than zero"
#endif

#if MIPSEL_EMU_CONSOLE_LINE_SIZE < 16
#error "MIPSEL_EMU_CONSOLE_LINE_SIZE must be at least 16"
#endif

#if MIPSEL_EMU_CONSOLE_OUTPUT_SIZE < 96
#error "MIPSEL_EMU_CONSOLE_OUTPUT_SIZE must be at least 96"
#endif

#if MIPSEL_EMU_UART_IRQ_LINE > 7
#error "MIPSEL_EMU_UART_IRQ_LINE must select CP0 interrupt input 0..7"
#endif

#if MIPSEL_EMU_INITRAMFS_ALIGNMENT == 0 || \
    (MIPSEL_EMU_INITRAMFS_ALIGNMENT & (MIPSEL_EMU_INITRAMFS_ALIGNMENT - 1u))
#error "MIPSEL_EMU_INITRAMFS_ALIGNMENT must be a power of two"
#endif

#endif
