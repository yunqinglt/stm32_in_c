#ifndef _OP_H
#define _OP_H

#include "config.h"
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


// posedge of clock -> tail of an inf loop
__STATIC_FORCEINLINE void decrease_random(Registers *state) {
    uint32_t random = state->cp0.byname.cp0r1_t.cp0r1_n.Random & 0x3fu;
    uint32_t wired = state->cp0.byname.cp0r6_t.cp0r6_n.Wired & 0x3fu;

    state->cp0.byname.cp0r1_t.cp0r1_n.Random =
        random <= wired ? 63u : random - 1u;
}

__STATIC_FORCEINLINE bool increase_counter(Registers *state) {
    state->cp0_count_divider += 1u;
    if (state->cp0_count_divider < MIPSEL_EMU_CP0_COUNT_DIVIDER)
        return false;

    state->cp0_count_divider = 0;
    state->cp0.byname.cp0r9_t.cp0r9_n.Count += 1u;
    return true;
}


#endif
