#include "instru.h"
#include "registers.h"
#include "exception.h"
#include <stdint.h>

// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    Features: 
    [*] single pipeline
    [/] basic mips32 release 2 behavior
    [/] Cache and Sync
    [*] branch delay
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
extern uint32_t read32(uint32_t addr);

// For dispatched implementation
void execute_instr(uint32_t instr, Registers *state) {
    MIPS_Instruction_Handler handler = op_table[getop(instr)];
    handler(instr, state);
}

void cpu_step(Registers *state) {
    uint32_t instr;

    if (state->pc & 0x03) {
        raise_exception(state, state->pc, EXC_AdEL, MIPS_VECTOR_GENERAL);
        instr = read32(state->pc); // exception vector
    }

    Result pa = pfn_translate(state->pc, state, 0);
    if (!TEST_RESULT(pa)) {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, state->pc, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, state->pc, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
        instr = read32(state->pc); // exception vector
    } else {
        instr = read32((uint32_t) pa.value.ok);
    }

    execute_instr(instr, state);

    if (state->is_delay_slot) {
        if (state->is_taken) {
            state->pc = state->target_pc; // branch taken
        } else {
            state->pc += 4; // branch not taken
        }

        state->is_delay_slot = 0;
        state->is_taken = 0;
    } else {
        state->pc = state->next_pc; // not branch
    }

    state->next_pc += 4;
}

void startup(Registers *state) {
    while (1) {
        if (status->state == RUNNING) {
            cpu_step(state);
        }
        else if (status->state == STEPPING) {
            if (status->steps != 0) {
                cpu_step(state);
                status->steps -= 1;
            }
            else continue;
        } else continue;
    }
}