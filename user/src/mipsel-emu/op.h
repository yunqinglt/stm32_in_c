#ifndef _OP_H
#define _OP_H

#include "registers.h"
#include "instru.h"

// Sign extend
#define sign_extend(imm) ((int32_t)(int16_t)(imm))

// Zero extend
#define zero_extend(offset)    \
    (offset & 0x0000ffff)

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

// CP0 C[25] == 1 -> table0 else table1
#define CFLAG(instr)    ((instr >> 25) & 0x01)
#define getsel(instr)   ((instr >> 0) & 0x07) // sel[2:0]


// $zero Specialization
#define S0_IS_0(state)  ((state)->gpr[0] = 0)

void op_j(uint32_t instr, Registers *state);
void op_jal(uint32_t instr, Registers *state);
void op_beq(uint32_t instr, Registers *state);
void op_bne(uint32_t instr, Registers *state);
void op_blez(uint32_t instr, Registers *state);
void op_bgtz(uint32_t instr, Registers *state);
void op_bgez(uint32_t instr, Registers *state);

void op_slti(uint32_t instr, Registers *state);
void op_sltiu(uint32_t instr, Registers *state);
void op_andi(uint32_t instr, Registers *state);
void op_ori(uint32_t instr, Registers *state);
void op_addi(uint32_t instr, Registers *state);
void op_xori(uint32_t instr, Registers *state);
void op_lui(uint32_t instr, Registers *state);




void op_srl(uint32_t instr, Registers *state);
void op_addu(uint32_t instr, Registers *state);
void op_move_from_hi(uint32_t instr, Registers *state);
void op_move_from_lo(uint32_t instr, Registers *state);
void op_addiu(uint32_t instr, Registers *state);
void op_multu(uint32_t instr, Registers *state);

void delta(uint32_t instr, Registers *state);
void beta(uint32_t instr, Registers *state);

void regimm_handler(uint32_t instr, Registers *state);
void special1_handler(uint32_t instr, Registers *state);
void op_cop0_handler(uint32_t instr, Registers *state);

void op_tlbr(uint32_t instr, Registers *state);
void op_tlbwi(uint32_t instr, Registers *state);
void op_tlbwr(uint32_t instr, Registers *state);
void op_tlbp(uint32_t instr, Registers *state);

void op_mfc0(uint32_t instr, Registers *state);
void op_mtc0(uint32_t instr, Registers *state);
__ALIAS("delta") void op_rdpgpr(uint32_t instr, Registers *state);
__ALIAS("delta") void op_wrpgpr(uint32_t instr, Registers *state); // TODO
void op_mfmc0(uint32_t instr, Registers *state);

// posedge of clock -> tail of an inf loop
__STATIC_FORCEINLINE void decrease_random(Registers *state) {
    if ((state->cp0.byname.cp0r1_t.cp0r1_n.Random <= (state->cp0.byname.cp0r6_t.cp0r6_n.Wired & 0x3f)) ||
        (state->cp0.byname.cp0r1_t.cp0r1_n.Random >= 63))
        state->cp0.byname.cp0r1_t.cp0r1_n.Random = 63;
    else state->cp0.byname.cp0r1_t.cp0r1_n.Random -= 1;
}

#endif