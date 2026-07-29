#include "op.h"

void op_addu(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = state->gpr[rs] + state->gpr[rt];
    S0_IS_0(state);
}

void op_move_from_hi(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);

    state->gpr[rd] = state->hi;
    S0_IS_0(state);
}

void op_move_from_lo(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);

    state->gpr[rd] = state->lo;
    S0_IS_0(state);
}

void op_srl(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rt = getrt(instr);
    uint8_t mask = getmask(instr);

    // To be implemented
}

void special1_handler(uint32_t instr, Registers *state) {
    uint8_t funct = getfunc(instr);

    MIPS_Instruction_Handler handler = special1_table[funct];

    
}