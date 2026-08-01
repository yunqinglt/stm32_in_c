#include "registers.h"

// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    Features: 
    [*] single pipeline
    [/] basic mips32 release 2 behavior
    [ ] branch delay
    [ ] virtual mmio device
    [ ] -   Console
    [ ] -   Display
    [ ] shadow register set
    [ ] virtual debug support
    [ ] -   GDB
    [-] FPU

    So hard!!!!!!!!
    [-] 5-way pipeline simulation
*/

extern vmstate_t *status;

// For dispatched implementation
extern uint32_t read32(uint32_t addr);
extern uint16_t read16(uint32_t addr);
extern uint8_t read8(uint32_t addr);

extern void write32(uint32_t addr, uint32_t data);
extern void write16(uint32_t addr, uint16_t data);
extern void write8(uint32_t addr, uint8_t data);

void startup(Registers *state) {
    while (1) {
        if (status->state == RUNNING) {/* Do something */}
        // Execute ( read32 ( state->pc ) , state )
        // state->pc = state->next_pc;
        // state->next_pc += 4;

        else if (status->state == STEPPING) {
            if (status->steps != 0) status->steps -= 1;
            else continue;
        } else continue;

    }
}