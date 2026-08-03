#include "op.h"

// Near 256MB Jump
void op_j(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    state->next_pc = (((state->pc + 4) & 0xf0000000) | (target << 2)); // fetch 31..28 of delay slot PC
}

// Near 256MB Jump and place return address
void op_jal(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    state->gpr[31] = state->pc + 4;
    state->next_pc = (((state->pc + 4) & 0xf0000000) | (target << 2));
}

// Branch on Equal
void op_beq(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] == state->gpr[rt]) {
        state->next_pc = state->pc + 4 + sign_extend(imm);
    }
}

void op_bne(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] != state->gpr[rt]) {
        state->next_pc = state->pc + 4 + sign_extend(imm);
    }
}

void op_blez(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if ((int32_t)state->gpr[rs] <= 0) {
        state->next_pc = state->pc + 4 + sign_extend(imm);
    }
}

void op_bgtz(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if ((int32_t)state->gpr[rs] > 0) {
        state->next_pc = state->pc + 4 + sign_extend(imm);
    }
}

void op_bgez(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if ((int32_t)state->gpr[rs] >= 0) {
        state->next_pc = state->pc + 4 + sign_extend(imm);
    }
}

void op_slti(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    int32_t imm = sign_extend(getimm(instr));

    state->gpr[rt] = ((int32_t)(state->gpr[rs]) < imm);
    S0_IS_0(state);
}

void op_sltiu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t imm = sign_extend(getimm(instr));

    state->gpr[rt] = (state->gpr[rs] < imm);
    S0_IS_0(state);
}

void op_andi(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t imm = zero_extend(getimm(instr));

    state->gpr[rt] = (state->gpr[rs] & imm);
    S0_IS_0(state);
}

void op_ori(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t imm = zero_extend(getimm(instr));

    state->gpr[rt] = (state->gpr[rs] | imm);
    S0_IS_0(state);
}

// void op_addi

void op_addu(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = (state->gpr[rs] + state->gpr[rt]);
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

    state->gpr[rd] = state->gpr[rt] >> mask;
    S0_IS_0(state);
}

void op_subu(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = state->gpr[rs] - state->gpr[rt];
    S0_IS_0(state);
}

void op_addi(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    int32_t imm = sign_extend(getimm(instr));

    if ((imm > 0 && state->gpr[rs] > INT32_MAX - imm) || (imm < 0 && state->gpr[rs] < INT32_MIN - imm)) {
        trigger_exception_helper(EXC_Ov, state, 0);
    } else {
        state->gpr[rt] = state->gpr[rs] + imm;
    }
    S0_IS_0(state);
}

void op_xori(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    state->gpr[rt] = state->gpr[rs] ^ (uint32_t) zero_extend(imm);
    S0_IS_0(state);
}

void op_lui(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    state->gpr[rt] = (uint32_t) (imm << 16);
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


// Reserved Instruction
void beta(uint32_t instr, Registers *state) {
    trigger_exception_helper(EXC_RI, state, 0);
}

// Coprocessor Unusable
void delta(uint32_t instr, Registers *state) {
    uint8_t cop_id = getop(instr) & 0x03;
    trigger_exception_helper(EXC_CpU, state, cop_id);
}

void regimm_handler(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);

    MIPS_Instruction_Handler handler = regimm_table[rt];

    // 100% Hit
    handler(instr, state);
}

void op_cop0_handler(uint32_t instr, Registers *state) {
    if (CFLAG(instr) == 1)
        MIPS_Instruction_Handler handler = cop0_table0[getfunc(instr)]; // FUNC[5:0]
    else
        MIPS_Instruction_Handler handler = cop0_table1[getrs(instr)]; // RS[25:21]

    handler(instr, state);
}





void op_mfc0(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t sel = getsel(instr);

    state->gpr[rt] = state->cp0[rd][sel];
    S0_IS_0(state);
}

void op_mtc0(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t sel = getsel(instr);

    state->cp0[rd][sel] = state->gpr[rt];
}

// Enable and disable interrupts
void op_mfmc0(uint32_t instr, Registers *state) {
    uint8_t func = getfunc(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    if (rd != 12) trigger_exception_helper(EXC_CpU, state, 0);

    state->gpr[rt] = state->cp0.byname.cp0r12_t.cp0r12_n.Status;
    if ((func >> 5) & 0x01) 
        state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
            SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status,
                CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN, 1);
    else
        state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
            SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status,
                CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN, 1);

    S0_IS_0(state);
}

void special1_handler(uint32_t instr, Registers *state) {
    uint8_t funct = getfunc(instr);

    MIPS_Instruction_Handler handler = special1_table[funct];

    // 100% Hit
    handler(instr, state);
}