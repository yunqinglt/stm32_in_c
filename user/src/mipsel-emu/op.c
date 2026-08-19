#include "op.h"
#include "compiler.h"
#include "exception.h"
#include "instru.h"
#include "platform.h"
#include "registers.h"
#include <stdint.h>
#include <sys/types.h>

extern MIPS_Instruction_Handler regimm_table[];
extern MIPS_Instruction_Handler special1_table[];
extern MIPS_Instruction_Handler special2_table[];
extern MIPS_Instruction_Handler special3_table[];
extern MIPS_Instruction_Handler cop0_table0[];
extern MIPS_Instruction_Handler cop0_table1[];

__STATIC_FORCEINLINE void nop(uint32_t instr, Registers *state) {}

__ALIAS("nop") void op_cache(uint32_t instr, Registers *state);
__ALIAS("nop") void op_sync(uint32_t instr, Registers *state);
__ALIAS("nop") void op_synci(uint32_t instr, Registers *state);
__ALIAS("nop") void op_pref(uint32_t instr, Registers *state);

__STATIC_FORCEINLINE void unconditional_branch(Registers *state) {
    state->is_delay_slot = 1;
    state->is_taken = 1;
}

__STATIC_FORCEINLINE uint32_t branch_target(const Registers *state,
                                            uint16_t immediate) {
    return state->pc + 4u + (uint32_t)(sign_extend(immediate) * 4);
}


// Near 256MB Jump
void op_j(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    unconditional_branch(state);
    state->target_pc = (((state->pc + 4) & 0xf0000000) | (target << 2)); // fetch 31..28 of delay slot PC
}

// Near 256MB Jump and place return address
void op_jal(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    unconditional_branch(state);
    state->gpr[31] = state->pc + 8;
    state->target_pc = (((state->pc + 4) & 0xf0000000) | (target << 2));
}

// Branch on Equal
void op_beq(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1; // execute branch delay
    if (state->gpr[rs] == state->gpr[rt]) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_bne(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1;
    if (state->gpr[rs] != state->gpr[rt]) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_blez(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] <= 0) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_bgtz(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] > 0) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_bltz(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] < 0) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_bltzl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    if ((int32_t)state->gpr[rs] < 0) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bgezl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    if ((int32_t)state->gpr[rs] >= 0) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bgezal(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->gpr[31] = state->pc + 8;
    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] >= 0) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_bltzal(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->gpr[31] = state->pc + 8;
    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] < 0) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_bgezall(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    state->gpr[31] = state->pc + 8;
    if ((int32_t)state->gpr[rs] >= 0) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bltzall(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    state->gpr[31] = state->pc + 8;
    if ((int32_t)state->gpr[rs] < 0) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bgez(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] >= 0) {
        state->is_taken = 1;
        state->target_pc = branch_target(state, imm);
    } else {
        state->is_taken = 0;
    }
}

void op_jalr(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t rd = getrd(instr);
    uint32_t target = state->gpr[rs];

    uint32_t ret_addr = state->pc + 8;
    state->gpr[rd] = ret_addr | state->ISAMode;

    S0_IS_0(state);

    state->is_delay_slot = 1;
    state->is_taken = 1;
    /* A misaligned target faults on the target fetch, after the delay slot. */
    state->target_pc = target;
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
    uint8_t rotate = getrs(instr);
    uint32_t value = state->gpr[rt];

    /* In Release 2, SPECIAL/SRL uses rs=0 and ROTR uses rs=1. */
    if (rotate > 1u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    if (rotate == 1u && mask != 0u) {
        state->gpr[rd] = (value >> mask) | (value << (32u - mask));
    } else {
        state->gpr[rd] = value >> mask;
    }
    S0_IS_0(state);
}

void op_subu(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = state->gpr[rs] - state->gpr[rt];
    S0_IS_0(state);
}

void op_and(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = (state->gpr[rs] & state->gpr[rt]);
    S0_IS_0(state);
}

void op_or(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = (state->gpr[rs] | state->gpr[rt]);
    S0_IS_0(state);
}

void op_xor(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = (state->gpr[rs] ^ state->gpr[rt]);
    S0_IS_0(state);
}

void op_slt(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if ((int32_t) state->gpr[rs] < (int32_t) state->gpr[rt]) {
        state->gpr[rd] = 1;
    } else {
        state->gpr[rd] = 0;
    }
}

void op_sltu(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if (state->gpr[rs] < state->gpr[rt]) {
        state->gpr[rd] = 1;
    } else {
        state->gpr[rd] = 0;
    }
}

void op_nor(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    state->gpr[rd] = ~(state->gpr[rs] | state->gpr[rt]);
    S0_IS_0(state);
}


void op_tge(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if ((int32_t) state->gpr[rs] >= (int32_t) state->gpr[rt]) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tgeu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if (state->gpr[rs] >= state->gpr[rt]) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tlt(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if ((int32_t) state->gpr[rs] < (int32_t) state->gpr[rt]) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tltu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if (state->gpr[rs] < state->gpr[rt]) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_teq(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if (state->gpr[rs] == state->gpr[rt]) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tne(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if (state->gpr[rs] != state->gpr[rt]) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tgei(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if ((int32_t) state->gpr[rs] >= sign_extend(imm)) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tgeiu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] >= (uint32_t) sign_extend(imm)) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tlti(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if ((int32_t) state->gpr[rs] < sign_extend(imm)) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tltiu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] < (uint32_t) sign_extend(imm)) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_teqi(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] == (uint32_t) sign_extend(imm)) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_tnei(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    if (state->gpr[rs] != (uint32_t) sign_extend(imm)) {
        raise_exception(state, 0, EXC_Tr, MIPS_VECTOR_GENERAL);
    }
}

void op_addi(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    int32_t value = (int32_t)state->gpr[rs];
    int32_t imm = sign_extend(getimm(instr));

    if ((imm > 0 && value > INT32_MAX - imm) ||
        (imm < 0 && value < INT32_MIN - imm)) {
        raise_exception(state, 0, EXC_Ov, MIPS_VECTOR_GENERAL);
        return;
    } else {
        state->gpr[rt] = (uint32_t)(value + imm);
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

    state->gpr[rt] = (uint32_t)imm << 16;
    S0_IS_0(state);
}

void op_beql(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t offset = getimm(instr);

    if (state->gpr[rs] == state->gpr[rt]) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bnel(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t offset = getimm(instr);

    if (state->gpr[rs] != state->gpr[rt]) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_blezl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    if ((int32_t)state->gpr[rs] <= 0) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bgtzl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    if ((int32_t)state->gpr[rs] > 0) {
        unconditional_branch(state);
        state->target_pc = branch_target(state, offset);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_jalx(uint32_t instr, Registers *state) {
    uint32_t target = gettar(instr);

    unconditional_branch(state);
    state->gpr[31] = state->pc + 8;
    state->target_pc = (state->pc & 0xf0000000) | (target << 2);

    state->ISAMode = state->ISAMode ^ 0x01;
}

// load byte
void op_lb(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr); // Base
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = offset + state->gpr[rs];

    // if (va & 0x01) {
    //     trigger_exception_helper(EXC_AdEL, state, va);
    //     return;
    // }

    // Little Endian
    Result pa = pfn_translate(va, state, 0);
    if (TEST_RESULT(pa)) {
        uint32_t data = (uint32_t)(int32_t)(int8_t) (read8((uint32_t) pa.value.ok));
        state->gpr[rt] = data;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
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

    if (va & 0x01) {
        raise_exception(state, va, EXC_AdEL, MIPS_VECTOR_GENERAL);
        return;
    }

    // Little Endian
    Result pa = pfn_translate(va, state, 0);

    if (TEST_RESULT(pa)) {
        uint32_t data = sign_extend(read16((uint32_t) pa.value.ok));
        state->gpr[rt] = data;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_lwl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    uint32_t aligned_va = va & ~0x03;
    uint32_t byte_offset = va & 0x03;

    Result pa = pfn_translate(aligned_va, state, 0);

    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        uint32_t reg_val = state->gpr[rt];
        
        switch (byte_offset) {
            case 0: reg_val = (reg_val & 0x00ffffff) | (word << 24); break;
            case 1: reg_val = (reg_val & 0x0000ffff) | (word << 16); break;
            case 2: reg_val = (reg_val & 0x000000ff) | (word << 8);  break;
            case 3: reg_val = word; break;
        }
        state->gpr[rt] = reg_val;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_lwr(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    uint32_t aligned_va = va & ~0x03;
    uint32_t byte_offset = va & 0x03;

    Result pa = pfn_translate(aligned_va, state, 0);

    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        uint32_t reg_val = state->gpr[rt];
        
        switch (byte_offset) {
            case 0: reg_val = word; break;
            case 1: reg_val = (reg_val & 0xff000000) | (word >> 8);  break;
            case 2: reg_val = (reg_val & 0xffff0000) | (word >> 16); break;
            case 3: reg_val = (reg_val & 0xffffff00) | (word >> 24); break;
        }
        state->gpr[rt] = reg_val;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_lbu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    Result pa = pfn_translate(va, state, 0);

    if (TEST_RESULT(pa)) {
        uint8_t byte = read8((uint32_t) pa.value.ok);
        state->gpr[rt] = zero_extend(byte);
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_lhu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;
    
    if (va & 0x01) {
        raise_exception(state, va, EXC_AdEL, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 0);

    if (TEST_RESULT(pa)) {
        uint16_t half = read16((uint32_t) pa.value.ok);
        state->gpr[rt] = zero_extend(half);
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_lw(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;
    
    if (va & 0x03) {
        raise_exception(state, va, EXC_AdEL, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 0);
    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        state->gpr[rt] = word;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_sb(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    Result pa = pfn_translate(va, state, 1);

    if (TEST_RESULT(pa)) {
        write8((uint32_t) pa.value.ok, (uint8_t)(state->gpr[rt] & 0xff));
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_MOD, MIPS_VECTOR_GENERAL);
                break;
        }
    }
}

void op_sh(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    if (va & 0x01) {
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 1);
    if (TEST_RESULT(pa)) {
        write16((uint32_t) pa.value.ok, (uint16_t)(state->gpr[rt] & 0xffff));
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_MOD, MIPS_VECTOR_GENERAL);
                break;
        }
    }
}

void op_sw(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    if (va & 0x03) {
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 1);
    if (TEST_RESULT(pa)) {
        write32((uint32_t) pa.value.ok, state->gpr[rt]);
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_MOD, MIPS_VECTOR_GENERAL);
                break;
        }
    }
}

void op_swl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    uint32_t aligned_va = va & ~0x03;
    uint32_t byte_offset = va & 0x03;

    Result pa = pfn_translate(aligned_va, state, 1);
    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        uint32_t reg = state->gpr[rt];

        switch (byte_offset) {
            case 0: word = ((word & 0xffffff00) | (reg >> 24)); break;
            case 1: word = ((word & 0xffff0000) | (reg >> 16)); break;
            case 2: word = ((word & 0xff000000) | (reg >> 8)); break;
            case 3: word = reg; break;
        }

        write32((uint32_t) pa.value.ok, word);
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_MOD, MIPS_VECTOR_GENERAL);
                break;
        }
    }
}

void op_swr(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    uint32_t aligned_va = va & ~0x03;
    uint32_t byte_offset = va & 0x03;

    Result pa = pfn_translate(aligned_va, state, 1);
    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        uint32_t reg = state->gpr[rt];

        switch (byte_offset) {
            case 0: word = reg; break;
            case 1: word = ((word & 0x000000ff) | (reg << 8)); break;
            case 2: word = ((word & 0x0000ffff) | (reg << 16)); break;
            case 3: word = ((word & 0x00ffffff) | (reg << 24)); break;
        }

        write32((uint32_t) pa.value.ok, word);
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_MOD, MIPS_VECTOR_GENERAL);
                break;
        }
    }
}

void op_ll(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;
    
    if (va & 0x03) {
        raise_exception(state, va, EXC_AdEL, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 0);
    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        state->gpr[rt] = word;
        state->ll_bit = 1;
        state->ll_addr = (uint32_t)pa.value.ok;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBL, MIPS_VECTOR_GENERAL);
                break;
        }
    }

    S0_IS_0(state);
}

void op_sc(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint32_t offset = sign_extend(getimm(instr));

    uint32_t va = state->gpr[rs] + offset;

    if (va & 0x03) {
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 1);
    if (TEST_RESULT(pa)) {
        uint32_t address = (uint32_t)pa.value.ok;
        uint32_t value = state->gpr[rt];
        bool succeeded = state->ll_bit && state->ll_addr == address;

        if (succeeded) write32(address, value);
        state->gpr[rt] = succeeded ? 1u : 0u;
        state->ll_bit = 0;
        state->ll_addr = 0;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_MOD, MIPS_VECTOR_GENERAL);
                break;
        }
    }
}

void op_sll(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t mask = getmask(instr);

    state->gpr[rd] = state->gpr[rt] << mask;
    S0_IS_0(state);
}

void op_sra(uint32_t instr, Registers *state) {
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t mask = getmask(instr) & 0x1f;

    state->gpr[rd] = (uint32_t) ((int32_t)state->gpr[rt] >> mask);
    S0_IS_0(state);
}

void op_sllv(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    state->gpr[rd] = state->gpr[rt] << (state->gpr[rs] & 0x1f);
    S0_IS_0(state);
}

void op_srlv(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);
    uint8_t rotate = getmask(instr);

    uint32_t mask = state->gpr[rs] & 0x1fu;
    uint32_t value = state->gpr[rt];

    /* In Release 2, SPECIAL/SRLV uses sa=0 and ROTRV uses sa=1. */
    if (rotate > 1u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    if (rotate == 1u && mask != 0u) {
        state->gpr[rd] = (value >> mask) | (value << (32u - mask));
    } else {
        state->gpr[rd] = value >> mask;
    }
    S0_IS_0(state);
}

void op_srav(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    state->gpr[rd] = (uint32_t) ((int32_t)state->gpr[rt] >> (state->gpr[rs] & 0x1f));
    S0_IS_0(state);
}

void op_jr(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint32_t target = state->gpr[rs];

    state->is_delay_slot = 1;
    state->is_taken = 1;
    /* A misaligned target faults on the target fetch, after the delay slot. */
    state->target_pc = target;
}

void op_movz(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    if (state->gpr[rt] == 0) {
        state->gpr[rd] = state->gpr[rs];
    }

    S0_IS_0(state);
}

void op_movn(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    if (state->gpr[rt] != 0) {
        state->gpr[rd] = state->gpr[rs];
    }

    S0_IS_0(state);
}

void op_syscall(uint32_t instr, Registers *state) {
    raise_exception(state, 0, EXC_SC, MIPS_VECTOR_GENERAL);
}

void op_break(uint32_t instr, Registers *state) {
    raise_exception(state, 0, EXC_BP, MIPS_VECTOR_GENERAL);
}

void op_move_to_hi(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    state->hi = state->gpr[rs];
}

void op_move_to_lo(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    state->lo = state->gpr[rs];
}

void op_mult(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    if (getrd(instr) != 0u || getmask(instr) != 0u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    int64_t prod = (int64_t)(int32_t)state->gpr[rs] *
                   (int64_t)(int32_t)state->gpr[rt];
    state->hi = (uint32_t)((uint64_t)prod >> 32);
    state->lo = (uint32_t)prod;
}

void op_div(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    int32_t num = (int32_t)state->gpr[rs];
    int32_t den = (int32_t)state->gpr[rt];

    if (den == 0) {
        return;
    }

    if (num == (int32_t)0x80000000 && den == -1) {
        state->lo = 0x80000000;
        state->hi = 0;
        return;
    }

    state->lo = (uint32_t)(num / den);
    state->hi = (uint32_t)(num % den);
}

void op_divu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    uint32_t num = state->gpr[rs];
    uint32_t den = state->gpr[rt];

    if (den == 0) {
        return;
    }

    state->lo = num / den;
    state->hi = num % den;
}

void op_add(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    int32_t a = (int32_t)state->gpr[rs];
    int32_t b = (int32_t)state->gpr[rt];

    if ((a > 0 && b > INT32_MAX - a) ||
        (a < 0 && b < INT32_MIN - a)) {
        raise_exception(state, 0, EXC_Ov, MIPS_VECTOR_GENERAL);
        return;
    } else {
        if (rd != 0) {
            state->gpr[rd] = (uint32_t)(a + b);
        }
    }

    S0_IS_0(state);
}

void op_sub(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    uint32_t a = state->gpr[rs];
    uint32_t b = state->gpr[rt];
    uint32_t res = a - b;

    if (((a ^ b) & (a ^ res)) & 0x80000000U) {
        raise_exception(state, 0, EXC_Ov, MIPS_VECTOR_GENERAL);
        return;
    }

    if (rd != 0) {
        state->gpr[rd] = res;
    }

    S0_IS_0(state);
}


void op_madd(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    int64_t a = (int32_t)state->gpr[rs];
    int64_t b = (int32_t)state->gpr[rt];

    int64_t prod = a * b;

    int64_t acc = (int64_t)(((uint64_t)state->hi << 32) | (uint32_t)state->lo);

    int64_t res = acc + prod;

    state->hi = (uint32_t)((uint64_t)res >> 32);
    state->lo = (uint32_t)(res & 0xFFFFFFFFULL);
}

void op_maddu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    uint64_t a = (uint32_t)state->gpr[rs];
    uint64_t b = (uint32_t)state->gpr[rt];

    uint64_t prod = a * b;

    uint64_t acc = ((uint64_t)state->hi << 32) | (uint32_t)state->lo;

    uint64_t res = acc + prod;

    state->hi = (uint32_t)(res >> 32);
    state->lo = (uint32_t)(res & 0xFFFFFFFFULL);
}

void op_mul(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    int64_t prod = (int64_t) state->gpr[rs] * (int64_t) state->gpr[rt];

    state->gpr[rd] = (uint32_t) (prod & 0xffffffff);
    S0_IS_0(state);
}

void op_msub(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    int64_t a = (int32_t)state->gpr[rs];
    int64_t b = (int32_t)state->gpr[rt];

    int64_t prod = a * b;

    int64_t acc = (int64_t)(((uint64_t)state->hi << 32) | (uint32_t)state->lo);

    int64_t res = acc - prod;

    state->hi = (uint32_t)((uint64_t)res >> 32);
    state->lo = (uint32_t)(res & 0xFFFFFFFFULL);
}

void op_msubu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);

    uint64_t a = (uint32_t)state->gpr[rs];
    uint64_t b = (uint32_t)state->gpr[rt];

    uint64_t prod = a * b;

    uint64_t acc = ((uint64_t)state->hi << 32) | (uint32_t)state->lo;

    uint64_t res = acc - prod;

    state->hi = (uint32_t)(res >> 32);
    state->lo = (uint32_t)(res & 0xFFFFFFFFULL);
}

// counter
void op_clo(uint32_t instr, Registers *state) {
    uint32_t count = 0;
    uint8_t rs = getrs(instr);
    uint8_t rd = getrd(instr);

    /* CLZ/CLO duplicate the destination register in rt and rd. */
    if (getrt(instr) != rd || getmask(instr) != 0u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    uint32_t value = state->gpr[rs];
    for (uint32_t bit = UINT32_C(0x80000000);
         bit != 0 && (value & bit) != 0; bit >>= 1) {
        ++count;
    }

    state->gpr[rd] = count;
    S0_IS_0(state);
}

void op_clz(uint32_t instr, Registers *state) {
    uint32_t count = 0;
    uint8_t rs = getrs(instr);
    uint8_t rd = getrd(instr);

    /* CLZ/CLO duplicate the destination register in rt and rd. */
    if (getrt(instr) != rd || getmask(instr) != 0u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    uint32_t value = state->gpr[rs];
    for (uint32_t bit = UINT32_C(0x80000000);
         bit != 0 && (value & bit) == 0; bit >>= 1) {
        ++count;
    }

    state->gpr[rd] = count;
    S0_IS_0(state);
}


void op_ext(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t msbd = getrd(instr);
    uint8_t lsb = getmask(instr);
    uint32_t size = (uint32_t)msbd + 1u;
    uint32_t mask;

    if ((uint32_t)lsb + size > 32u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }
    mask = size == 32u ? UINT32_MAX : (UINT32_C(1) << size) - 1u;
    state->gpr[rt] = (state->gpr[rs] >> lsb) & mask;

    S0_IS_0(state);
}

void op_ins(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t msb = getrd(instr);
    uint8_t lsb = getmask(instr);
    uint32_t width;
    uint32_t field_mask;

    if (msb < lsb) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }
    width = (uint32_t)msb - lsb + 1u;
    field_mask = width == 32u
        ? UINT32_MAX : ((UINT32_C(1) << width) - 1u) << lsb;
    state->gpr[rt] = (state->gpr[rt] & ~field_mask) |
                     ((state->gpr[rs] << lsb) & field_mask);

    S0_IS_0(state);
}

void op_addiu(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    /* ADDIU sign-extends the immediate but still wraps modulo 2^32. */
    state->gpr[rt] = state->gpr[rs] + (uint32_t)sign_extend(imm);
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
    raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
}

// Coprocessor Unusable
void delta(uint32_t instr, Registers *state) {
    uint8_t cop_id = getop(instr) & 0x03;
    raise_exception(state, cop_id, EXC_CpU, MIPS_VECTOR_GENERAL);
}

void op_deret(uint32_t instr, Registers *state) {
    delta(instr, state);
}

void op_wfe(uint32_t instr, Registers *state) {
    /* A simple single-threaded model treats WAIT as an idle cycle. The main
     * loop keeps Count and device IRQs advancing between instructions. */
    (void)instr;
    (void)state;
}

void op_rdpgpr(uint32_t instr, Registers *state) {
    delta(instr, state);
}

void op_wrpgpr(uint32_t instr, Registers *state) {
    delta(instr, state);
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
    uint8_t found = 0;

    for (int i = 0; i < 64; ++i) {
        uint32_t pmask = state->tlb[i].pmask;
        uint32_t ehi   = state->tlb[i].entryhi;
        uint32_t elo0  = state->tlb[i].entrylo0;
        uint32_t elo1  = state->tlb[i].entrylo1;

        uint32_t mask = ~(pmask | 0x1FFF);

        if ((state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi & mask) == (ehi & mask)) {
            uint8_t is_global = (elo0 & 1) & (elo1 & 1);
            if (is_global || (ehi & 0xFF) == current_asid) {
                matched = (uint8_t)i;
                found = 1;
                break;
            }
        }
    }

    if (found) {
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
        state->next_pc = state->cp0.byname.cp0r14_t.cp0r14_n.EPC;
        state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
            SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_EXL_POS, CP0_STATUS_EXL_LEN, 0);
    }

    state->ll_bit = 0;
    state->ll_addr = 0;
    state->is_delay_slot = 0;
    state->is_taken = 0;
    state->target_pc = 0;
    state->bds = 0;
    state->exception_pending = 0;
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
    uint32_t value = state->gpr[rt];

    if (rd == 13 && sel == 0) {
        /* Cause only exposes software interrupt and control bits as RW. */
        const uint32_t writable =
            (UINT32_C(1) << CP0_CAUSE_DC_POS) |
            (UINT32_C(1) << CP0_CAUSE_PCI_POS) |
            (UINT32_C(1) << CP0_CAUSE_IV_POS) |
            (UINT32_C(1) << CP0_CAUSE_WP_POS) |
            (UINT32_C(3) << CP0_CAUSE_IP_POS);
        uint32_t old = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
            (old & ~writable) | (value & writable);
    } else {
        state->cp0.regs[rd][sel] = value;
    }

    /* Writing Compare acknowledges the MIPS Count/Compare interrupt. */
    if (rd == 11 && sel == 0) {
        uint32_t cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        cause = SET_BITFIELD(cause, CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN, 0);
        cause = SET_BITFIELD(cause, CP0_CAUSE_IP_POS + 7, 1, 0);
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause = cause;
    }
}

// Enable and disable interrupts
void op_mfmc0(uint32_t instr, Registers *state) {
    uint8_t func = getfunc(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    if (rd != 12) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    state->gpr[rt] = state->cp0.byname.cp0r12_t.cp0r12_n.Status;
    uint32_t ie_val = ((func >> 5) & 0x01) ? 1 : 0;
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
        SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status,\
            CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN, ie_val);

    S0_IS_0(state);
}

void op_wsbh(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rt = getrt(instr);

    if (rd == 0) return;

    uint32_t v = state->gpr[rt];
    state->gpr[rd] = ((v & 0x00FF00FF) << 8) | ((v & 0xFF00FF00) >> 8);
}

void op_seb(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rt = getrt(instr);

    if (getrs(instr) != 0u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    if (rd == 0) return;

    uint8_t temp = (uint8_t) (state->gpr[rt] & 0xffu);
    state->gpr[rd] = (uint32_t)(int32_t)(int8_t)temp;
}

void op_seh(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rt = getrt(instr);

    if (rd == 0) return;

    uint16_t temp = (uint16_t) (state->gpr[rt] & 0xffff);
    state->gpr[rd] = sign_extend(temp);
}

void op_bshfl(uint32_t instr, Registers *state) {
    uint8_t funct = getmask(instr);

    MIPS_Instruction_Handler handler = bshfl_table[funct];

    handler(instr, state);
}

void op_rdhwr(uint32_t instr, Registers *state) {
    uint8_t rd = getrd(instr);
    uint8_t rt = getrt(instr);

    if (getrs(instr) != 0u || getmask(instr) != 0u) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    /* Kernel mode may read implemented HWRs directly.  Other modes need the
     * corresponding CP0 HWREna bit, as required by MIPS32 Release 2. */
    if (!STATUS_EXL(state) && !STATUS_ERL(state) &&
        STATUS_KSU(state) != 0u &&
        !(state->cp0.byname.cp0r7_t.cp0r7_n.HWREna &
          (UINT32_C(1) << rd))) {
        raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
        return;
    }

    switch (rd) {
        case 0:
            state->gpr[rt] = 0; /* Single emulated CPU. */
            break;
        case 1:
            state->gpr[rt] = 32; /* SYNCI step, in bytes. */
            break;
        case 2:
            state->gpr[rt] = state->cp0.byname.cp0r9_t.cp0r9_n.Count;
            break;
        case 3:
            state->gpr[rt] = 1; // resolution of count
            break;

        default:
            /* HWR 4..29 are reserved here; 30 and 31 are optional
             * implementation-dependent registers which are not modeled. */
            raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
            return;
    }
    S0_IS_0(state);
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
    MIPS_Instruction_Handler handler;

    /* Kernel/EXL/ERL always has CP0 access; other modes require Status.CU0. */
    if (!STATUS_EXL(state) && !STATUS_ERL(state) &&
        STATUS_KSU(state) != 0 && !STATUS_CU0(state)) {
        raise_exception(state, 0, EXC_CpU, MIPS_VECTOR_GENERAL);
        return;
    }

    if (CFLAG(instr) == 1) {
        handler = cop0_table0[getfunc(instr)]; // FUNC[5:0]
    } else {
        handler = cop0_table1[getrs(instr)]; // RS[24:21]
    }

    handler(instr, state);
}
