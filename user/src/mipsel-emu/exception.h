#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include "registers.h"
#include <stdint.h>
#include <stdbool.h>

#define EXC_RESET   0xfe
#define EXC_SRES    0xf0

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

typedef enum {
    MIPS_VECTOR_GENERAL,
    MIPS_VECTOR_TLB_REFILL,
    MIPS_VECTOR_INTERRUPT,
    MIPS_VECTOR_CACHE_ERROR,
    MIPS_VECTOR_RESET,
} VectorClass;

void reset_cpu(Registers *state);

// TODO: Reset with secondary state?
extern void soft_reset_cpu(Registers *state);
extern void linux_load_reset(Registers *state);

#endif