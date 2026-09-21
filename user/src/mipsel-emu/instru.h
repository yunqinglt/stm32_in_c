#ifndef _INSTRU_H
#define _INSTRU_H

#include "compiler.h"
#include "registers.h"
#include "op.h"

#ifdef MIPSEL_EMU_DEFINE_INSTRUCTION_TABLES

MIPS_Instruction_Handler op_table[64] = {
    /* Entries are ordered by primary opcode (bits 31..26). */
    special1_handler, regimm_handler, op_j, op_jal,
    op_beq, op_bne, op_blez, op_bgtz,
    op_addi, op_addiu, op_slti, op_sltiu,
    op_andi, op_ori, op_xori, op_lui,
    op_cop0_handler, delta, delta, delta,
    op_beql, op_bnel, op_blezl, op_bgtzl,
    beta, beta, beta, beta,
    special2_handler, op_jalx, beta, special3_handler,
    op_lb, op_lh, op_lwl, op_lw,
    op_lbu, op_lhu, op_lwr, beta,
    op_sb, op_sh, op_swl, op_sw,
    beta, beta, op_swr, op_cache,
    op_ll, delta, delta, op_pref,
    beta, delta, delta, beta,
    op_sc, delta, delta, beta,
    beta, delta, delta, beta,
};

MIPS_Instruction_Handler special1_table[64] = {
    /* Entries are ordered by function field (bits 5..0). */
    op_sll, delta, op_srl, op_sra,
    op_sllv, beta, op_srlv, op_srav,
    op_jr, op_jalr, op_movz, op_movn,
    op_syscall, op_break, beta, op_sync,
    op_move_from_hi, op_move_to_hi, op_move_from_lo, op_move_to_lo,
    beta, beta, beta, beta,
    op_mult, op_multu, op_div, op_divu,
    beta, beta, beta, beta,
    op_add, op_addu, op_sub, op_subu,
    op_and, op_or, op_xor, op_nor,
    beta, beta, op_slt, op_sltu,
    beta, beta, beta, beta,
    op_tge, op_tgeu, op_tlt, op_tltu,
    op_teq, beta, op_tne, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
};

MIPS_Instruction_Handler regimm_table[32] = {
    op_bltz, op_bgez, op_bltzl, op_bgezl,
    beta, beta, beta, beta,
    op_tgei, op_tgeiu, op_tlti, op_tltiu,
    op_teqi, beta, op_tnei, beta,
    op_bltzal, op_bgezal, op_bltzall, op_bgezall,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, op_synci,
};

MIPS_Instruction_Handler special2_table[64] = {
    op_madd, op_maddu, op_mul, beta,
    beta, op_msub, op_msubu, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    op_clz, op_clo, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
};

MIPS_Instruction_Handler special3_table[64] = {
    op_ext, beta, beta, beta,
    op_ins, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    op_bshfl, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, op_rdhwr,
    beta, beta, beta, beta,
};

MIPS_Instruction_Handler bshfl_table[32] = {
    beta, beta, op_wsbh, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    op_seb, beta, beta, beta,
    beta, beta, beta, beta,
    op_seh, beta, beta, beta,
    beta, beta, beta, beta,
};

// | --- COP0 --- |C=1 | - All Zero - | Func |
//  < --  6   -- >< 1 >< ---  19  --- ><- 6 ->
MIPS_Instruction_Handler cop0_table0[64] = {
    beta, op_tlbr, op_tlbwi, beta,
    beta, beta, op_tlbwr, beta,
    op_tlbp, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    op_eret, beta, beta, beta,
    beta, beta, beta, op_deret,
    op_wfe, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
    beta, beta, beta, beta,
};

// | --- COP0 --- |C=0 | Func | rt | rd | All Zero | Sel |
// < --  6   --  >< 1 ><- 4 ->< 5 >< 5 >< -- 8 -- ><- 3 ->
MIPS_Instruction_Handler cop0_table1[16] = {
    op_mfc0, beta, beta, beta,
    op_mtc0, beta, beta, beta,
    beta, beta, op_rdpgpr, op_mfmc0,
    beta, beta, op_wrpgpr, beta,
};

#else

extern MIPS_Instruction_Handler op_table[64];
extern MIPS_Instruction_Handler special1_table[64];
extern MIPS_Instruction_Handler regimm_table[32];
extern MIPS_Instruction_Handler special2_table[64];
extern MIPS_Instruction_Handler special3_table[64];
extern MIPS_Instruction_Handler bshfl_table[32];
extern MIPS_Instruction_Handler cop0_table0[64];
extern MIPS_Instruction_Handler cop0_table1[16];

#endif

#endif
