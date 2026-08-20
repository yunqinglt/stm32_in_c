#ifndef MIPSEL_EMU_EMU_H
#define MIPSEL_EMU_EMU_H

#include "registers.h"
#include <stdint.h>

void execute_instr(uint32_t instr, Registers *state);
void cpu_step(Registers *state);
void update_cycle(Registers *state);

/* Portable cooperative execution API.  One step includes instruction or
 * exception processing plus one Count/device cycle.  The bounded run call is
 * intended to yield regularly to USB, DMA, and storage tasks on an MCU. */
void mipsel_emu_step(Registers *state);
uint32_t mipsel_emu_run_steps(Registers *state, uint32_t budget);

#endif
