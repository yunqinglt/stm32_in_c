#ifndef MIPSEL_EMU_PLATFORM_H
#define MIPSEL_EMU_PLATFORM_H

#include <stdint.h>

#define PLATFORM_MEMORY_SIZE (16u * 1024u + 1024u)

/* A single byte-addressed RAM instance owned by platform.c. */
extern uint8_t pool[PLATFORM_MEMORY_SIZE];

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
