#include "platform.h"
#include "registers.h"
#include <stdint.h>

uint8_t read8(uint32_t addr) {
    return (uint8_t) *(pool + addr);
}

uint16_t read16(uint32_t addr) {
    return (uint16_t) *(pool + addr);
}

uint32_t read32(uint32_t addr) {
    return (uint32_t) *(pool + addr);
}

void write8(uint32_t addr, uint8_t data) {
    *(pool + addr) = data;
}

void write16(uint32_t addr, uint16_t data) {
    *(pool + addr) = data;
}

void write32(uint32_t addr, uint32_t data) {
    *(pool + addr) = data;
}