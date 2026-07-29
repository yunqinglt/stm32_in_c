#include "instru.h"

MIPS_Instruction_Handler *target_handler(MIPS_Instruction_Handler *table, uint8_t Index) {
    if (Index > 64) return &
}

// Reserved Instruction Exception
void RI_exception(uint32_t instr, Registers *state) {
    
}