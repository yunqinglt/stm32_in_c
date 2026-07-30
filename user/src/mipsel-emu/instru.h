#ifndef _INSTRU_H
#define _INSTRU_H

#include "compiler.h"
#include "registers.h"
#include "op.h"

#define EXC_INT     0x00
#define EXC_MOD     0x01
#define EXC_TLBL    0x02
#define EXC_TLBS    0x03
#define EXC_AdEL    0x04
#define EXC_AdES    0x05

#define EXC_IBE     0x06 // Bus Error Instruction
#define EXC_DBE     0x07 // Data

#define EXC_SC      0x08 // SysCall
#define EXC_BP      0x09 // not willing to implement jtag debug

#define EXC_RI      0x0a // Reserved Instruction
#define EXC_CpU     0x0b // Unusable Coprocessor
#define EXC_Ov      0x0c // Overflow
#define EXC_Tr      0x0d // Trap

#define EXC_FPE     0x0f // TODO: Float Point

#define EXC_C2E     0x12 // Not implemented
#define EXC_DSP     0x16

#define EXC_WATCH   0x17
#define EXC_MCheck  0x18

#define EXC_THR     0x19
#define EXC_CAH     0x1e


MIPS_Instruction_Handler op_table[64] = {
    [0x00] = special1_handler,
    [0x01] = regimm_handler,
    [0x02] = op_j,
    [0x03] = op_jal,
    [0x04] = op_beq,
    [0x05] = op_bne,
    [0x06] = op_blez,
    [0x07] = op_bgtz,

    [0x08] = op_addi,
    [0x09] = op_addiu,  // rt = rs + imm
    [0x0a] = op_slti,
    [0x0b] = op_sltiu,
    [0x0c] = op_andi,
    [0x0d] = op_ori,
    [0x0e] = op_xori,
    [0x0f] = op_lui,

    [0x10] = op_cop0_handler,
    
    [0x11] = delta, // Unusable Coprocessor
    [0x12] = delta,
    [0x13] = delta,

    [0x14] = op_beql,
    [0x15] = op_bnel,
    [0x16] = op_blezl,
    [0x17] = op_bgtzl,

    [0x18] = beta, // Reversed Instruction
    [0x19] = beta,
    [0x1a] = beta,
    [0x1b] = beta,

    [0x1c] = special2_handler,
    [0x1d] = op_jalx,

    [0x1e] = beta,

    [0x1f] = special3_handler,

    [0x20] = op_lb,
    
};

MIPS_Instruction_Handler special1_table[64] = {
    [0x00] = op_sll,
    [0x02] = op_srl,          // rd <- 0^mask | rt[31..mask]
    [0x03] = op_sra,
    [0x04] = op_sllv,

    [0x06] = op_srlv,
    [0x07] = op_srav,


    [0x08] = op_jr,
    [0x09] = op_jalr,
    [0x0a] = op_movz,
    [0x0b] = op_movn,
    [0x0c] = op_syscall,
    [0x0d] = op_break,

    [0x0f] = op_sync,


    [0x10] = op_move_from_hi, // rd <- hi
    [0x11] = op_move_to_hi,
    [0x12] = op_move_from_lo, // rd <- lo

    [0x18] = op_mult,
    [0x19] = op_multu,
    // t <- rs * rt
    // lo = t[31..0]
    // hi = t[63..32]
    [0x1a] = op_div,
    [0x1b] = op_divu,

    [0x20] = op_add,    // rd <- rs + rt, cause exception, not modified
    [0x21] = op_addu,   // rd <- rs + rt, not cause exception
    [0x22] = op_sub,    // rd <- rs - rt
    [0x23] = op_subu,

    [0x2a] = op_slt,
    [0x2b] = op_sltu,

};


// | --- COP0 --- |C=1 | - All Zero - | Func |
//  < --  6   -- >< 1 >< ---  19  --- ><- 6 ->
MIPS_Instruction_Handler cop0_table0[64] {
    [0x01] = op_tlbr,
    [0x02] = op_tlbwi,
    [0x06] = op_tlbwr,
    [0x08] = op_tlbp,

    [0x18] = op_eret,

    [0x1f] = op_deret, // Debug Exception Return
    [0x20] = op_wfe,
};

// | --- COP0 --- |C=0 | Func | rt | rd | All Zero | Sel |
// < --  6   --  >< 1 ><- 4 ->< 5 >< 5 >< -- 8 -- ><- 3 ->
MIPS_Instruction_Handler cop0_table1[16] {
    [0x00] = op_mfc0,
    [0x04] = op_mtc0,
};

MIPS_Instruction_Handler *target_handler(MIPS_Instruction_Handler *table, uint8_t Index);
void RI_exception(uint32_t instr, Registers *state);
void SC_exception(uint32_t instr, Registers *state);

#endif