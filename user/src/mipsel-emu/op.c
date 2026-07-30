#include "op.h"

// Near 256MB Jump
void op_j(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    state->pc = ((state->pc & 0xf0000000) | (target << 2)); // only fetch the 31..28 of PC
}

// Near 256MB Jump and place return address
void op_jal(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    state->gpr[31] = state->pc + 8;
    state->pc = ((state->pc & 0xf0000000) | (target << 2));
}

// Branch on Equal
void op_beq(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] == state->gpr[rt]) {
        state->pc = state->pc + 4 + sign_extend(imm);
    }
}

void op_bne(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] != state->gpr[rt]) {
        state->pc = state->pc + 4 + sign_extend(imm);
    }
}

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

void op_subu(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = state->gpr[rs] - state->gpr[rt];
    S0_IS_0(state);
}

void op_addiu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    state->gpr[rt] = state->gpr[rs] + imm;
    S0_IS_0(state);
}

void op_multu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint64_t tmp = state->gpr[rs] * state->gpr[rt];

    state->hi = (uint32_t) (tmp >> 32);
    state->lo = (uint32_t) (tmp & 0xffffffff);
}

void delta(uint32_t instr, Registers *state) {
    uint8_t opcode = instr >> 26;
    uint8_t cop_id = opcode & 0x03;

    state->cp0.byname.cp0r13_t.cp0r13_n.Cause &= ~((0x1f << 2) | (0x3 << 28)); // ExcCode & Coprocessor number
    
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause |= (EXC_CpU << 2);
    
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause |= (cop_id << 28);

    state->cp0.byname.cp0r12_t.cp0r12_n.Status |= (1 << 1);
    state->cp0.byname.cp0r14_t.cp0r14_n.EPC = state->pc;
    
    state->pc = 0x80000180;
}

void regimm_handler(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);

    MIPS_Instruction_Handler handler = regimm_table[rt];

    // 100% Hit
    handler(instr, state);
}


void special1_handler(uint32_t instr, Registers *state) {
    uint8_t funct = getfunc(instr);

    MIPS_Instruction_Handler handler = special1_table[funct];

    // 100% Hit
    handler(instr, state);
}