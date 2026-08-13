#include "registers.h"

// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    Features: 
    [*] single pipeline
    [/] basic mips32 release 2 behavior
    [ ] branch delay
    [ ] virtual mmio device
    [ ] ->   UART Console
    [ ] ->   Display
    [ ] Multi-Thread Application-Specific Extension
    [ ] ->   shadow register set
    [ ] SmartMIPS Application-Specific Extension
    [ ] virtual debug support
    [ ] ->   GDB

    [-] FPU
    [-] XPA eXtended Physical Address
    [-] 5-way pipeline simulation
    
    [*] VPS Variable Page Sizes
*/

extern vmstate_t *status;

// For dispatched implementation

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