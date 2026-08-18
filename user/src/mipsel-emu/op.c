#include "op.h"
#include "compiler.h"
#include "exception.h"
#include "platform.h"
#include "registers.h"
#include <signal.h>
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
    state->next_pc = (((state->pc + 4) & 0xf0000000) | (target << 2));
}

// Branch on Equal
void op_beq(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1; // execute branch delay
    if (state->gpr[rs] == state->gpr[rt]) {
        state->is_taken = 1;
        state->next_pc = state->pc + 4 + (sign_extend(imm) << 2);
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
        state->next_pc = state->pc + 4 + (sign_extend(imm) << 2);
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
        state->next_pc = state->pc + 4 + (sign_extend(imm) << 2);
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
        state->next_pc = state->pc + 4 + (sign_extend(imm) << 2);
    } else {
        state->is_taken = 0;
    }
}

void op_bgez(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t imm = getimm(instr);

    state->is_delay_slot = 1;
    if ((int32_t)state->gpr[rs] >= 0) {
        state->is_taken = 1;
        state->next_pc = state->pc + 4 + (sign_extend(imm) << 2);
    } else {
        state->is_taken = 0;
    }
}

void op_jalr(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t rd = getrd(instr);
    uint32_t target = state->gpr[rs];

    if ((target & 0x01) && (!CONF1_CA(state))) {
        raise_exception(state, target, EXC_AdEL, MIPS_VECTOR_GENERAL);
        return;
    }

    uint32_t ret_addr = state->pc + 8;
    state->gpr[rd] = ret_addr | state->ISAMode;

    S0_IS_0(state);

    state->is_delay_slot = 1;
    state->is_taken = 1;
    state->target_pc = target & ~0x01;
    
    state->ISAMode = target & 0x01;
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
        raise_exception(state, 0, EXC_Ov, MIPS_VECTOR_GENERAL);
        return;
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

void op_beql(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint16_t offset = getimm(instr);

    if (state->gpr[rs] == state->gpr[rt]) {
        unconditional_branch(state);
        state->target_pc = state->pc + 4 + (sign_extend(offset) << 2);
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

    if (state->gpr[rs] == state->gpr[rt]) {
        unconditional_branch(state);
        state->target_pc = state->pc + 4 + (sign_extend(offset) << 2);
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
        state->target_pc = state->pc + 4 + (sign_extend(offset) << 2);
    } else {
        state->is_delay_slot = 0;
        state->is_taken = 0;
        state->next_pc = state->pc + 8;
    }
}

void op_bgtzl(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint16_t offset = getimm(instr);

    if ((int32_t)state->gpr[rs] >= 0) {
        unconditional_branch(state);
        state->target_pc = state->pc + 4 + (sign_extend(offset) << 2);
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

    state->ISAMode ^= state->ISAMode;
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
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
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
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
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
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
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
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
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
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
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
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
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
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
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
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
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
        raise_exception(state, va, EXC_AdES, MIPS_VECTOR_GENERAL);
        return;
    }

    Result pa = pfn_translate(va, state, 0);
    if (TEST_RESULT(pa)) {
        uint32_t word = read32((uint32_t) pa.value.ok);
        state->gpr[rt] = word;
        state->ll_bit = 1;

        // state->ll_addr?
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
        write32((uint32_t) pa.value.ok, state->gpr[rt]);
        state->ll_bit = 0;
    } else {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_TLB_REFILL);
                break;
            case 3:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
                break;
            case 4:
                raise_exception(state, va, EXC_TLBS, MIPS_VECTOR_GENERAL);
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

    state->gpr[rd] = state->gpr[rt] >> (state->gpr[rs] & 0x1f);
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

    if ((target & 0x01) && (!CONF1_CA(state))) {
        raise_exception(state, target, EXC_AdEL, MIPS_VECTOR_GENERAL);
        return;
    }

    S0_IS_0(state);

    state->is_delay_slot = 1;
    state->is_taken = 1;
    state->target_pc = target & ~0x01;
    
    state->ISAMode = target & 0x01;
}

void op_movz(uint32_t instr, Registers *state) {
    uint8_t rs = getrs(instr);
    uint8_t rt = getrt(instr);
    uint8_t rd = getrd(instr);

    
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
    raise_exception(state, 0, EXC_RI, MIPS_VECTOR_GENERAL);
}

// Coprocessor Unusable
void delta(uint32_t instr, Registers *state) {
    uint8_t cop_id = getop(instr) & 0x03;
    raise_exception(state, cop_id, EXC_CpU, MIPS_VECTOR_GENERAL);
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
        state->next_pc = state->cp0.byname.cp0r14_t.cp0r14_n.EPC;
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
        raise_exception(state, 0, EXC_CpU, MIPS_VECTOR_GENERAL);
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
        state->gpr[rd] = sign_extend((int16_t)(uint8_t) state->gpr[rt]);
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