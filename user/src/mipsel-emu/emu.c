#include "instru.h"
#include "op.h"
#include "registers.h"
#include "exception.h"
#include <stdint.h>

// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    Features: 
    [*] single pipeline
    [*] basic mips32 release 2 behavior
    [x] Cache and Sync (C-based emulator has no hazard)
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

static void commit_pending_exception(Registers *state) {
    state->pc = state->next_pc;
    state->next_pc = state->pc + 4u;
    state->exception_pending = 0;
}

void cpu_step(Registers *state) {
    uint32_t instr;

    /* Also handles an interrupt or other exception raised between steps. */
    if (state->exception_pending) {
        commit_pending_exception(state);
        return;
    }

    if (state->pc & 0x03) {
        raise_exception(state, state->pc, EXC_AdEL, MIPS_VECTOR_GENERAL);
        commit_pending_exception(state);
        return;
    }

    Result pa = pfn_translate(state->pc, state, 0);
    if (!TEST_RESULT(pa)) {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, state->pc, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                commit_pending_exception(state);
                return;
            case 3:
                raise_exception(state, state->pc, EXC_TLBL, MIPS_VECTOR_GENERAL);
                commit_pending_exception(state);
                return;
        }
    }

    instr = read32((uint32_t) pa.value.ok);
    execute_instr(instr, state);

    /* A synchronous instruction exception wins over normal/branch commit. */
    if (state->exception_pending) {
        commit_pending_exception(state);
        return;
    }

    state->bds = 0;

    if (state->is_delay_slot) {
        state->bds = 1;

        if (state->is_taken) {
            state->pc = state->next_pc; // branch taken
            state->next_pc = state->target_pc;
            state->is_delay_slot = 0;
            state->is_taken = 0;
            return;
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

void update_cycle(Registers *state) {
    decrease_random(state);
    increase_counter(state);
}

void startup(Registers *state) {
    while (1) {
        if (status->state == RESET) {
            reset_cpu(state);
            status->state = RUNNING;
        }
        else if (status->state == RUNNING) {
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
