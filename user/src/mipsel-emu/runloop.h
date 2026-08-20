#ifndef MIPSEL_EMU_RUNLOOP_H
#define MIPSEL_EMU_RUNLOOP_H

#include "registers.h"

/* POSIX debugger/frontend loop.  Embedded firmware uses mipsel_emu_step(). */
int startup(Registers *state);

#endif
