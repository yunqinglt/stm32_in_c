#ifndef MIPSEL_EMU_RUNLOOP_H
#define MIPSEL_EMU_RUNLOOP_H

#include "registers.h"

typedef void (*mipsel_runloop_service_fn)(void *opaque);

/* POSIX debugger/frontend loop.  Embedded firmware uses mipsel_emu_step(). */
int startup(Registers *state);
void mipsel_runloop_set_service(mipsel_runloop_service_fn service,
                                 void *opaque);

#endif
