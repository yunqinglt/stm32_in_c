#ifndef MIPSEL_EMU_BOARD_CH32V203F6P6_H
#define MIPSEL_EMU_BOARD_CH32V203F6P6_H

/* APS6404L is a 64-Mbit (8 MiB) external PSRAM.  It is the guest RAM; the
 * CH32V203's 10 KiB internal SRAM remains available to the MCU runtime,
 * USB/SPI/UART service code, and the emulator's Registers object. */
#define MIPSEL_EMU_RAM_SIZE (8u * 1024u * 1024u)

/* The maintenance transport drains these queues from the MCU's USB task. */
#define MIPSEL_EMU_UART_RX_FIFO_SIZE 4u
#define MIPSEL_EMU_UART_TX_FIFO_SIZE 4u

/* A fixed boot package has no reason to reserve the desktop-sized limits. */
#define MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS 8u
#define MIPSEL_EMU_IMAGE_CHUNK_SIZE 128u

/* The GUI framebuffer is disabled by CONFIG_IS_EMBEDDED_SYSTEM. */
#define MIPSEL_EMU_ENABLE_FRAMEBUFFER 0

#endif
