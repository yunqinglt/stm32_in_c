#ifndef MIPSEL_EMU_PLATFORM_H
#define MIPSEL_EMU_PLATFORM_H

#include "registers.h"
#include "uart16550.h"
#include <stdbool.h>
#include <stdint.h>

#define PLATFORM_MEMORY_SIZE (16u * 1024u * 1024u)

void platform_init(uart16550_tx_callback_t uart_tx, void *opaque);
void platform_reset(void);
bool platform_uart_can_receive(void);
bool platform_uart_receive(uint8_t byte);
void platform_update_interrupts(Registers *state);

/* The frontend supplies one byte-addressed RAM instance to platform.c. */
/*
 * Little-endian physical-memory accessors. Until bus-error propagation is
 * added, out-of-range reads return zero and out-of-range writes are ignored.
 */
uint8_t read8(uint32_t addr);
uint16_t read16(uint32_t addr);
uint32_t read32(uint32_t addr);

void write8(uint32_t addr, uint8_t data);
void write16(uint32_t addr, uint16_t data);
void write32(uint32_t addr, uint32_t data);

#endif
