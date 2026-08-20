#ifndef MIPSEL_EMU_H
#define MIPSEL_EMU_H

/* Single public include for embedded firmware integrations. */
#include "config.h"
#if MIPSEL_EMU_ENABLE_CONSOLE
#include "console.h"
#endif
#include "disasm.h"
#include "emu.h"
#include "exception.h"
#include "image_loader.h"
#include "observer.h"
#include "platform.h"
#include "registers.h"
#if MIPSEL_EMU_ENABLE_UART16550
#include "uart16550.h"
#endif

#endif
