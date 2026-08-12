#include "op.h"
#include "exception.h"
#include "registers.h"
#include <stdint.h>

extern MIPS_Instruction_Handler regimm_table[];
extern MIPS_Instruction_Handler special1_table[];
extern MIPS_Instruction_Handler special2_table[];
extern MIPS_Instruction_Handler special3_table[];
extern MIPS_Instruction_Handler cop0_table0[];
extern MIPS_Instruction_Handler cop0_table1[];

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

// TODO: branch likely

// load byte
void op_lb(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr); // Base
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = offset + state->gpr[rs];

    // Little Endian
    Result pa = pfn_translate(va, state, 0);
    if (TEST_RESULT(pa)) {
        uint32_t data = sign_extend(read8((uint32_t) pa.value.ok));
        state->gpr[rt] = data;
    } else {
        switch (pa.value.reason) {
            case 1:
                trigger_exception_helper(EXC_AdEL, state, va);
                break;
            case 2:
            case 3:
                trigger_exception_helper(EXC_TLBL, state, va);
                break;
        }
    }

    S0_IS_0(state);
}

// load half
void op_lh(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr); // Base
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = offset + state->gpr[rs];

    // Little Endian
    Result pa = pfn_translate(va, state, 0);
    if (TEST_RESULT(pa)) {
        uint32_t data = sign_extend(read16((uint32_t) pa.value.ok));
        state->gpr[rt] = data;
    } else {
        switch (pa.value.reason) {
            case 1:
                trigger_exception_helper(EXC_AdEL, state, va);
                break;
            case 2:
            case 3:
                trigger_exception_helper(EXC_TLBL, state, va);
                break;
        }
    }

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

void op_tlbr(uint32_t instr, Registers *state) {
    uint8_t index = (state->cp0.byname.cp0r0_t.cp0r0_n.Index & 0x0000003f); // [5:0]

    if (index < 64) {
        uint8_t g = (state->tlb[index].entrylo0 & 1) && (state->tlb[index].entrylo1 & 1);

        state->cp0.regs[5][0] = state->tlb[index].pmask; // PageMask
        state->cp0.regs[10][0] = state->tlb[index].entryhi; // EntryHi

        state->cp0.regs[2][0] = (state->tlb[index].entrylo0 & ~1U) | g; // EntryLo0
        state->cp0.regs[3][0] = (state->tlb[index].entrylo1 & ~1U) | g; // EntryLo1
    }   // else = undefined -> do nothing
}

void op_tlbwi(uint32_t instr, Registers *state) {
    uint8_t index = (state->cp0.byname.cp0r0_t.cp0r0_n.Index & 0x0000003f); // [5:0]

    if (index < 64) {
        uint32_t pmask = state->cp0.regs[5][0];
        uint8_t g = (state->cp0.regs[2][0] & 1) && (state->cp0.regs[3][0] & 1);

        state->tlb[index].pmask = pmask;
        state->tlb[index].entryhi = state->cp0.regs[10][0] & ~(pmask & 0x1fffe000);

        state->tlb[index].entrylo0 = (state->cp0.regs[2][0] & ~1U) | g;
        state->tlb[index].entrylo1 = (state->cp0.regs[3][0] & ~1U) | g;
    }
}

void op_tlbwr(uint32_t instr, Registers *state) {
    uint8_t index = (state->cp0.byname.cp0r1_t.cp0r1_n.Random & 0x3f);

    if (index < 64) {
        uint32_t pmask = state->cp0.regs[5][0];
        uint8_t g = (state->cp0.regs[2][0] & 1) && (state->cp0.regs[3][0] & 1);

        state->tlb[index].pmask = pmask;
        state->tlb[index].entryhi = state->cp0.regs[10][0] & ~(pmask & 0x1fffe000);

        state->tlb[index].entrylo0 = (state->cp0.regs[2][0] & ~1U) | g;
        state->tlb[index].entrylo1 = (state->cp0.regs[3][0] & ~1U) | g;
    }
}

// Probe TLB for matching entry
void op_tlbp(uint32_t instr, Registers *state) {
    uint8_t current_asid = state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi & 0xFF;
    uint8_t matched = 0;

    for (int i = 0; i < 64; ++i) {
        uint32_t pmask = state->tlb[i].pmask;
        uint32_t ehi   = state->tlb[i].entryhi;
        uint32_t elo0  = state->tlb[i].entrylo0;
        uint32_t elo1  = state->tlb[i].entrylo1;

        uint32_t mask = ~(pmask | 0x1FFF);

        if ((state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi & mask) == (ehi & mask)) {
            uint8_t is_global = (elo0 & 1) & (elo1 & 1);
            if (is_global || (ehi & 0xFF) == current_asid) {
                matched = i;
            }
        }
    }

    if (matched != 0) {
        state->cp0.byname.cp0r0_t.cp0r0_n.Index = 
            SET_BITFIELD(state->cp0.byname.cp0r0_t.cp0r0_n.Index, 0, 6, matched);
        state->cp0.byname.cp0r0_t.cp0r0_n.Index = 
            SET_BITFIELD(state->cp0.byname.cp0r0_t.cp0r0_n.Index, 31, 1, 0);
    }
    else {
        // Undefined -> The index field left its value
        state->cp0.byname.cp0r0_t.cp0r0_n.Index = 
            SET_BITFIELD(state->cp0.byname.cp0r0_t.cp0r0_n.Index, 31, 1, 1);
    }
}

void op_eret(uint32_t instr, Registers *state) {
    if (STATUS_ERL(state) == 1) {
        state->next_pc = state->cp0.byname.cp0r30_t.cp0r30_n.ErrorEPC;
        state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
            SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_ERL_POS, CP0_STATUS_ERL_LEN, 0);
        // Clear flag
    } else {
        state->next_pc = state->cp0.byname.cp0r30_t.cp0r30_n.ErrorEPC;
        state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
            SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_EXL_POS, CP0_STATUS_EXL_LEN, 0);
    }
}



void op_mfc0(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t sel = getsel(instr);

    state->gpr[rt] = state->cp0.regs[rd][sel];
    S0_IS_0(state);
}

void op_mtc0(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t sel = getsel(instr);

    state->cp0.regs[rd][sel] = state->gpr[rt];
}

// Enable and disable interrupts
void op_mfmc0(uint32_t instr, Registers *state) {
    uint8_t func = getfunc(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    if (rd != 12) {
        trigger_exception_helper(EXC_CpU, state, 0);
    }

    state->gpr[rt] = state->cp0.byname.cp0r12_t.cp0r12_n.Status;
    uint32_t ie_val = ((func >> 5) & 0x01) ? 1 : 0;
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
        SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status,\
            CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN, ie_val);

    S0_IS_0(state);
}

void op_bshfl(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    if (getrs(instr) == 0x10) {
        // SEB rd, rt:  GPR[rd] <- SignExtend(GPR[rt][7:0])
        state->gpr[rd] = sign_extend((uint8_t) state->gpr[rt]);
    }
}

void special1_handler(uint32_t instr, Registers *state) {
    uint8_t funct = getfunc(instr);

    MIPS_Instruction_Handler handler = special1_table[funct];

    // 100% Hit
    handler(instr, state);
}

void special2_handler(uint32_t instr, Registers *state) {
    uint8_t funct = getfunc(instr);

    MIPS_Instruction_Handler handler = special2_table[funct];

    // 100% Hit
    handler(instr, state);
}

void special3_handler(uint32_t instr, Registers *state) {
    uint8_t funct = getfunc(instr);

    MIPS_Instruction_Handler handler = special3_table[funct];

    // 100% Hit
    handler(instr, state);
}

void op_cop0_handler(uint32_t instr, Registers *state) {
    MIPS_Instruction_Handler handler = cop0_table1[getrs(instr)]; // RS[25:21]

    if (CFLAG(instr) == 1)
        handler = cop0_table0[getfunc(instr)]; // FUNC[5:0]

    handler(instr, state);
}