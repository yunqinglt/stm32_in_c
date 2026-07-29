#ifndef _INSTRU_H
#define _INSTRU_H

#include "compiler.h"
#include "registers.h"
#include "op.h"

MIPS_Instruction_Handler op_table[64] = {
    [0x00] = special1_handler,

    [0x02] = op_j,
    [0x03] = op_jal,
    [0x04] = op_beq,
    [0x05] = op_bne,
    [0x08] = op_addi,
    [0x09] = op_addiu,

    [0x0a] = op_slti,
    [0x0b] = op_sltiu,

    [0x0f] = op_lui,
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
    [0x0c] = op_syscall,

    [0x10] = op_move_from_hi, // rd <- hi
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

#endif