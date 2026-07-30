#include "registers.h"

extern vmstate_t *status;

// For user's implementation
extern uint32_t read32(uint32_t addr);
extern uint16_t read16(uint32_t addr);
extern uint8_t read8(uint32_t addr);

extern void write32(uint32_t addr, uint32_t data);
extern void write16(uint32_t addr, uint16_t data);
extern void write8(uint32_t addr, uint8_t data);

void startup(Registers *state) {
    while (1) {
        if (status->state == RUNNING) {/* Do something */}
        else if (status->state == STEPPING) {
            if (status->steps != 0) status->steps -= 1;
            else continue;
        } else continue;

    }
}