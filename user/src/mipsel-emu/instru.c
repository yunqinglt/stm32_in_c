#include "instru.h"

MIPS_Instruction_Handler target_handler(MIPS_Instruction_Handler *table, uint8_t Index) {
    MIPS_Instruction_Handler handler = table[Index];

    if (handler == NULL) return RI_exception;

    return handler;
}

// Reserved Instruction Exception: 0x0a
// TODO: Branch Delay
void RI_exception(uint32_t instr, Registers *state) {
    (void) instr;

    state->cp0.byname.cp0r13_t.cp0r13_n.Cause &= 0xffffff83;
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause |= (0x0a << 2);


    state->cp0.byname.cp0r12_t.cp0r12_n.Status |= (1 << 1);
    state->cp0.byname.cp0r14_t.cp0r14_n.EPC = state->pc;
    // epc = pc - 4
    state->pc = 0x80000180;
}

// SysCall Exception: 0x08
void SC_exception(uint32_t instr, Registers *state) {
    (void) instr;

    state->cp0.byname.cp0r13_t.cp0r13_n.Cause &= 0xffffff83;
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause |= (0x08 << 2);

    state->cp0.byname.cp0r12_t.cp0r12_n.Status |= (1 << 1);
    state->cp0.byname.cp0r14_t.cp0r14_n.EPC = state->pc;
    // epc = pc - 4
    state->pc = 0x80000180;
}

// Integer Overflow Exception: 0x0c
void OV_exception(uint32_t instr, Registers *state) {
    (void) instr;

    state->cp0.byname.cp0r13_t.cp0r13_n.Cause &= 0xffffff83;
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause |= (0x0c << 2);

    state->cp0.byname.cp0r12_t.cp0r12_n.Status |= (1 << 1);
    state->cp0.byname.cp0r14_t.cp0r14_n.EPC = state->pc;
    // epc = pc - 4
    state->pc = 0x80000180;
}

void Reset_exception(uint32_t instr, Registers *state) {
    (void) instr;

    
}