#ifndef _INSTRU_H
#define _INSTRU_H

#include "compiler.h"
#include "registers.h"
#include "op.h"


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

    [0x14] = op_beql, // delayed branch -> skip slot instructions
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
    [0x21] = op_lh,
    [0x22] = op_lwl,
    [0x23] = op_lw,
    [0x24] = op_lbu,
    [0x25] = op_lhu,
    [0x26] = op_lwr,
    [0x27] = beta,

    [0x28] = op_sb,
    [0x29] = op_sh,
    [0x2a] = op_swl,
    [0x2b] = op_sw,
    [0x2c] = beta,
    [0x2d] = beta,
    [0x2e] = op_swr,
    [0x2f] = op_cache, // complicated

    [0x30] = op_ll,
    [0x31] = delta,
    [0x32] = delta,
    [0x33] = op_perf,
    [0x34] = beta,
    [0x35] = delta,
    [0x36] = delta,
    [0x37] = beta,

    [0x38] = op_sc,
    [0x39] = delta,
    [0x3a] = delta,
    [0x3b] = beta,
    [0x3c] = beta,
    [0x3d] = delta,
    [0x3e] = delta,
    [0x3f] = beta,
};

MIPS_Instruction_Handler special1_table[64] = {
    [0x00] = op_sll,
    [0x01] = delta,
    [0x02] = op_srl,          // rd <- 0^mask | rt[31..mask]
    [0x03] = op_sra,
    [0x04] = op_sllv,
    [0x05] = beta,
    [0x06] = op_srlv,
    [0x07] = op_srav,

    [0x08] = op_jr,
    [0x09] = op_jalr,
    [0x0a] = op_movz,
    [0x0b] = op_movn,
    [0x0c] = op_syscall,
    [0x0d] = op_break,
    [0x0e] = beta,
    [0x0f] = op_sync,

    [0x10] = op_move_from_hi, // rd <- hi
    [0x11] = op_move_to_hi,
    [0x12] = op_move_from_lo, // rd <- lo
    [0x13] = op_move_to_lo,
    [0x14] = beta,
    [0x15] = beta,
    [0x16] = beta,
    [0x17] = beta,

    [0x18] = op_mult,
    [0x19] = op_multu,
    // t <- rs * rt
    // lo = t[31..0]
    // hi = t[63..32]
    [0x1a] = op_div,
    [0x1b] = op_divu,
    [0x1c] = beta,
    [0x1d] = beta,
    [0x1e] = beta,
    [0x1f] = beta,

    [0x20] = op_add,    // rd <- rs + rt, cause exception, not modified
    [0x21] = op_addu,   // rd <- rs + rt, not cause exception
    [0x22] = op_sub,    // rd <- rs - rt
    [0x23] = op_subu,
    [0x24] = op_and,    // Important
    [0x25] = op_or,
    [0x26] = op_xor,
    [0x27] = op_nor,

    [0x28] = beta,
    [0x29] = beta,
    [0x2a] = op_slt,
    [0x2b] = op_sltu,
    [0x2c] = beta,
    [0x2d] = beta,
    [0x2e] = beta,
    [0x2f] = beta,

    [0x30] = op_tge, // Trap
    [0x31] = op_tgeu,
    [0x32] = op_tlt,
    [0x33] = op_tltu,
    [0x34] = op_teq,
    [0x35] = beta,
    [0x36] = op_tne,
    [0x37] = beta,

    [0x38] = beta,
    [0x39] = beta,
    [0x3a] = beta,
    [0x3b] = beta,
    [0x3c] = beta,
    [0x3d] = beta,
    [0x3e] = beta,
    [0x3f] = beta,
};


MIPS_Instruction_Handler regimm_table[32] = {
    [0x00] = op_bltz,
    [0x01] = op_bgez,
    [0x02] = op_bltzl,
    [0x03] = op_bgezl,
    [0x04] = beta,
    [0x05] = beta,
    [0x06] = beta,
    [0x07] = beta,

    [0x08] = op_tgei,
    [0x09] = op_tgeiu,
    [0x0a] = op_tlti,
    [0x0b] = op_tltiu,
    [0x0c] = op_teqi,
    [0x0d] = beta,
    [0x0e] = op_tnei,
    [0x0f] = beta,

    [0x10] = op_bltzal,
    [0x11] = op_bgezal,
    [0x12] = op_bltzall,
    [0x13] = op_bgezall,
    [0x14] = beta,
    [0x15] = beta,
    [0x16] = beta,
    [0x17] = beta,

    [0x18] = beta,
    [0x19] = beta,
    [0x1a] = beta,
    [0x1b] = beta,
    [0x1c] = beta,
    [0x1d] = beta,
    [0x1e] = beta,
    [0x1f] = op_synci,
};


MIPS_Instruction_Handler special2_table[64] = {
    [0 ... 63] = beta,

    [0x00] = op_madd,
    [0x01] = op_maddu,
    [0x02] = op_mul,

    [0x05] = op_msub,
    [0x06] = op_msubu,

    [0x20] = op_clz,
    [0x21] = op_clo,
};


MIPS_Instruction_Handler special3_table[64] = {
    [0 ... 63] = beta,

    [0x00] = op_ext,

    [0x04] = op_ins,

    [0x20] = op_bshfl,

    [0x3b] = op_rdhwr,
};


// | --- COP0 --- |C=1 | - All Zero - | Func |
//  < --  6   -- >< 1 >< ---  19  --- ><- 6 ->
MIPS_Instruction_Handler cop0_table0[64] = {
    [0 .. 63] = delta,

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
MIPS_Instruction_Handler cop0_table1[16] = {
    // beta
    [0 .. 15] = delta,

    [0x00] = op_mfc0,
    [0x04] = op_mtc0,

    [0x0a] = op_rdpgpr,
    [0x0b] = op_mfmc0, // DI EI,

    [0x0e] = op_wrpgpr,

    // delta
};

#endif