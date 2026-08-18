#ifndef _OP_H
#define _OP_H

#include "compiler.h"
#include "registers.h"
// #include "instru.h"
#include "exception.h"
#include <stdint.h>

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
#define gettar(instr)   ((instr >> 0) & 0x03ffffff) // target[25..0]

// CP0 C[25] == 1 -> table0 else table1
#define CFLAG(instr)    ((instr >> 25) & 0x01)
#define getsel(instr)   ((instr >> 0) & 0x07) // sel[2:0]

// $zero Specialization
#define S0_IS_0(state)  ((state)->gpr[0] = 0)

#define TCregion(exp, lsb, msb) (lsb <= exp && exp <= msb)
#define CLregion(exp, lsb, msb) (lsb <= exp && exp < msb)
#define CRregion(exp, lsb, msb) (lsb < exp && exp <= msb)
#define TOregion(exp, lsb, msb) (lsb < exp && exp < msb)


void op_j(uint32_t instr, Registers *state);
void op_jal(uint32_t instr, Registers *state);
void op_beq(uint32_t instr, Registers *state);
void op_bne(uint32_t instr, Registers *state);
void op_blez(uint32_t instr, Registers *state);
void op_bgtz(uint32_t instr, Registers *state);
void op_bgez(uint32_t instr, Registers *state);

void op_jalr(uint32_t instr, Registers *state);
void op_jalx(uint32_t instr, Registers *state);

void op_slti(uint32_t instr, Registers *state);
void op_sltiu(uint32_t instr, Registers *state);
void op_andi(uint32_t instr, Registers *state);
void op_ori(uint32_t instr, Registers *state);
void op_addi(uint32_t instr, Registers *state);
void op_xori(uint32_t instr, Registers *state);
void op_lui(uint32_t instr, Registers *state);

void op_beql(uint32_t instr, Registers *state);
void op_bnel(uint32_t instr, Registers *state);
void op_blezl(uint32_t instr, Registers *state);
void op_bgtzl(uint32_t instr, Registers *state);

void op_bgezal(uint32_t instr, Registers *state);
void op_bltzal(uint32_t instr, Registers *state);
void op_bgezall(uint32_t instr, Registers *state);
void op_bltzall(uint32_t instr, Registers *state);

void op_pref(uint32_t instr, Registers *state);

void op_lb(uint32_t instr, Registers *state);
void op_lbu(uint32_t instr, Registers *state);
void op_lh(uint32_t instr, Registers *state);
void op_lw(uint32_t instr, Registers *state);
void op_lwl(uint32_t instr, Registers *state);
void op_lwr(uint32_t instr, Registers *state);
void op_lhu(uint32_t instr, Registers *state);

void op_sb(uint32_t instr, Registers *state);
void op_sh(uint32_t instr, Registers *state);
void op_sw(uint32_t instr, Registers *state);

void op_cache(uint32_t instr, Registers *state);
void op_ll(uint32_t instr, Registers *state);
void op_sc(uint32_t instr, Registers *state);
void op_sync(uint32_t instr, Registers *state);
void op_synci(uint32_t instr, Registers *state);

void op_srl(uint32_t instr, Registers *state);
void op_sll(uint32_t instr, Registers *state);
void op_sra(uint32_t instr, Registers *state);
void op_sllv(uint32_t instr, Registers *state);
void op_srlv(uint32_t instr, Registers *state);
void op_srav(uint32_t instr, Registers *state);
void op_jr(uint32_t instr, Registers *state);

void op_movz(uint32_t instr, Registers *state);
void op_movn(uint32_t instr, Registers *state);
void op_syscall(uint32_t instr, Registers *state);
void op_break(uint32_t instr, Registers *state);
void op_move_to_hi(uint32_t instr, Registers *state);
void op_move_to_lo(uint32_t instr, Registers *state);
void op_mult(uint32_t instr, Registers *state);
void op_div(uint32_t instr, Registers *state);
void op_divu(uint32_t instr, Registers *state);
void op_add(uint32_t instr, Registers *state);
void op_sub(uint32_t instr, Registers *state);
void op_subu(uint32_t instr, Registers *state);

void op_and(uint32_t instr, Registers *state);
void op_or(uint32_t instr, Registers *state);
void op_xor(uint32_t instr, Registers *state);
void op_nor(uint32_t instr, Registers *state);

void op_slt(uint32_t instr, Registers *state);
void op_sltu(uint32_t instr, Registers *state);

void op_tge(uint32_t instr, Registers *state);
void op_tgeu(uint32_t instr, Registers *state);
void op_tlt(uint32_t instr, Registers *state);
void op_tltu(uint32_t instr, Registers *state);
void op_teq(uint32_t instr, Registers *state);
void op_tne(uint32_t instr, Registers *state);

void op_swl(uint32_t instr, Registers *state);
void op_swr(uint32_t instr, Registers *state);

void op_addu(uint32_t instr, Registers *state);
void op_move_from_hi(uint32_t instr, Registers *state);
void op_move_from_lo(uint32_t instr, Registers *state);
void op_addiu(uint32_t instr, Registers *state);
void op_multu(uint32_t instr, Registers *state);

void op_clo(uint32_t instr, Registers *state);
void op_clz(uint32_t instr, Registers *state);

void op_madd(uint32_t instr, Registers *state);
void op_maddu(uint32_t instr, Registers *state);
void op_mul(uint32_t instr, Registers *state);
void op_msub(uint32_t instr, Registers *state);
void op_msubu(uint32_t instr, Registers *state);

void op_bltz(uint32_t instr, Registers *state);
void op_bltzl(uint32_t instr, Registers *state);
void op_bgezl(uint32_t instr, Registers *state);
void op_tgei(uint32_t instr, Registers *state);
void op_tgeiu(uint32_t instr, Registers *state);
void op_tlti(uint32_t instr, Registers *state);
void op_tltiu(uint32_t instr, Registers *state);
void op_teqi(uint32_t instr, Registers *state);
void op_tnei(uint32_t instr, Registers *state);
void op_ext(uint32_t instr, Registers *state);
void op_ins(uint32_t instr, Registers *state);

void delta(uint32_t instr, Registers *state);
void beta(uint32_t instr, Registers *state);

void op_wsbh(uint32_t instr, Registers *state);
void op_seb(uint32_t instr, Registers *state);
void op_seh(uint32_t instr, Registers *state);
void op_rdhwr(uint32_t instr, Registers *state);

void op_bshfl(uint32_t instr, Registers *state);
void regimm_handler(uint32_t instr, Registers *state);
void special1_handler(uint32_t instr, Registers *state);
void special2_handler(uint32_t instr, Registers *state);
void op_cop0_handler(uint32_t instr, Registers *state);
void special3_handler(uint32_t instr, Registers *state);

void op_tlbr(uint32_t instr, Registers *state);
void op_tlbwi(uint32_t instr, Registers *state);
void op_tlbwr(uint32_t instr, Registers *state);
void op_tlbp(uint32_t instr, Registers *state);
void op_eret(uint32_t instr, Registers *state);
void op_deret(uint32_t instr, Registers *state);
void op_wfe(uint32_t instr, Registers *state);


void op_mfc0(uint32_t instr, Registers *state);
void op_mtc0(uint32_t instr, Registers *state);
void op_rdpgpr(uint32_t instr, Registers *state);
void op_wrpgpr(uint32_t instr, Registers *state); // TODO
void op_mfmc0(uint32_t instr, Registers *state);

// posedge of clock -> tail of an inf loop
__STATIC_FORCEINLINE void decrease_random(Registers *state) {
    uint32_t random = state->cp0.byname.cp0r1_t.cp0r1_n.Random & 0x3fu;
    uint32_t wired = state->cp0.byname.cp0r6_t.cp0r6_n.Wired & 0x3fu;

    state->cp0.byname.cp0r1_t.cp0r1_n.Random =
        random <= wired ? 63u : random - 1u;
}

__STATIC_FORCEINLINE void increase_counter(Registers *state) {
    state->cp0.byname.cp0r9_t.cp0r9_n.Count += 1;
}


#endif
