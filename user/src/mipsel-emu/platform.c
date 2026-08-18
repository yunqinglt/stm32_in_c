#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

uint8_t pool[PLATFORM_MEMORY_SIZE];

// TODO
// readx(uint32_t addr, Registers *state)
// -> raise_exception(IBE)?
// -> raise_exception(MC)?

static bool address_range_valid(uint32_t addr, uint32_t width) {
    return width <= PLATFORM_MEMORY_SIZE &&
           addr <= PLATFORM_MEMORY_SIZE - width;
}

uint8_t read8(uint32_t addr) {
    if (!address_range_valid(addr, 1)) return 0;

    return pool[addr];
}

uint16_t read16(uint32_t addr) {
    if (!address_range_valid(addr, 2)) return 0;

    return (uint16_t) pool[addr] |
           ((uint16_t) pool[addr + 1] << 8);
}

uint32_t read32(uint32_t addr) {
    if (!address_range_valid(addr, 4)) return 0;

    return (uint32_t) pool[addr] |
           ((uint32_t) pool[addr + 1] << 8) |
           ((uint32_t) pool[addr + 2] << 16) |
           ((uint32_t) pool[addr + 3] << 24);
}

void write8(uint32_t addr, uint8_t data) {
    if (!address_range_valid(addr, 1)) return;

    pool[addr] = data;
}

void write16(uint32_t addr, uint16_t data) {
    if (!address_range_valid(addr, 2)) return;

    pool[addr] = (uint8_t) data;
    pool[addr + 1] = (uint8_t) (data >> 8);
}

void write32(uint32_t addr, uint32_t data) {
    if (!address_range_valid(addr, 4)) return;

    pool[addr] = (uint8_t) data;
    pool[addr + 1] = (uint8_t) (data >> 8);
    pool[addr + 2] = (uint8_t) (data >> 16);
    pool[addr + 3] = (uint8_t) (data >> 24);
}
