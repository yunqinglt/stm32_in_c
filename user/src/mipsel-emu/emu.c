#include "emu.h"
#include "instru.h"
#include "observer.h"
#include "op.h"
#include "registers.h"
#include "exception.h"
#include "platform.h"
#include <stdint.h>
// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    Features: 
    [*] single pipeline
    [*] basic mips32 release 2 behavior
    [x] Cache and Sync (C-based emulator has no hazard)
    [*] branch delay
    [*] virtual mmio device
    [*] ->   16550A UART Console
    [ ] ->   Display
    [ ] Multi-Thread Application-Specific Extension
    [ ] ->   shadow register set
    [ ] SmartMIPS Application-Specific Extension
    [*] virtual debug support
    [*] ->   ncurses register/instruction/exception TUI
    [ ] ->   GDB

    [-] FPU
    [-] XPA eXtended Physical Address
    [-] 5-way pipeline simulation
    
    [*] VPS Variable Page Sizes
*/

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

    /* Interrupts are sampled between instructions. */
    platform_update_interrupts(state);
    if (STATUS_IE(state) && !STATUS_EXL(state) && !STATUS_ERL(state) &&
        (STATUS_IM(state) & CAUSE_IP(state))) {
        raise_exception(state, 0, EXC_INT, MIPS_VECTOR_INTERRUPT);
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
    mipsel_emu_observer_instruction_begin(state->pc,
                                          (uint32_t)pa.value.ok, instr,
                                          state);
    execute_instr(instr, state);
    state->gpr[0] = 0;

    /* A synchronous instruction exception wins over normal/branch commit. */
    if (state->exception_pending) {
        commit_pending_exception(state);
        mipsel_emu_observer_instruction_end(state);
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
            mipsel_emu_observer_instruction_end(state);
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
    mipsel_emu_observer_instruction_end(state);
}

void update_cycle(Registers *state) {
    decrease_random(state);
    increase_counter(state);

    if (state->cp0.byname.cp0r9_t.cp0r9_n.Count ==
        state->cp0.byname.cp0r11_t.cp0r11_n.Compare) {
        uint32_t cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        cause = SET_BITFIELD(cause, CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN, 1);
        cause = SET_BITFIELD(cause, CP0_CAUSE_IP_POS + 7, 1, 1);
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause = cause;
    }
}

void mipsel_emu_step(Registers *state) {
    if (!state) return;
    cpu_step(state);
    update_cycle(state);
}

uint32_t mipsel_emu_run_steps(Registers *state, uint32_t budget) {
    uint32_t completed = 0;

    if (!state) return 0;
    while (completed < budget) {
        mipsel_emu_step(state);
        ++completed;
    }
    return completed;
}
