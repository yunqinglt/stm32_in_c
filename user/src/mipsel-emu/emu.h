#ifndef MIPSEL_EMU_EMU_H
#define MIPSEL_EMU_EMU_H

#include "registers.h"
#include <stdint.h>

void execute_instr(uint32_t instr, Registers *state);
void cpu_step(Registers *state);
void update_cycle(Registers *state);
int startup(Registers *state);

#endif
