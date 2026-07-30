#ifndef _OP_H
#define _OP_H

#include "registers.h"
#include "instru.h"

// Sign extend
#define sign_extend(offset)    \
    (offset >> 15) ? ((offset << 2) | 0xfffc0000) : ((offset << 2) | 0x00000000)

// R-Type
#define getop(instr)    (instr >> 26)           // op_code[31..26]
#define getrs(instr)    ((instr >> 21) & 0x1f)  // rs[25..21]
#define getrt(instr)    ((instr >> 16) & 0x1f)  // rt[20..16]
#define getrd(instr)    ((instr >> 11) & 0x1f)  // rd[15..11]
#define getmask(instr)  ((instr >> 6) & 0x1f)   // mask[10..6]
#define getfunc(instr)  ((instr >> 0) & 0x3f)   // funct[5..0]

// I-Type
#define getimm(instr)   ((instr >> 0) & 0xffff) // imm[15..0]

// J-Type
#define gettar(instr)   ((instr >> 0) & 0x03ffff) // target[25..0]

// $zero Specialization
#define S0_IS_0(state)  ((state)->gpr[0] = 0)

void op_j(uint32_t instr, Registers *state);
void op_jal(uint32_t instr, Registers *state);
void op_beq(uint32_t instr, Registers *state);
void op_bne(uint32_t instr, Registers *state);

void op_addu(uint32_t instr, Registers *state);
void op_move_from_hi(uint32_t instr, Registers *state);
void op_move_from_lo(uint32_t instr, Registers *state);
void op_addiu(uint32_t instr, Registers *state);
void op_multu(uint32_t instr, Registers *state);

void delta(uint32_t instr, Registers *state);

void regimm_handler(uint32_t instr, Registers *state);
void special1_handler(uint32_t instr, Registers *state);

#endif