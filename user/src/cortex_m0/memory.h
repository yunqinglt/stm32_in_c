#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include "compiler.h"

// Let LD tell us
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

// System Control Space
#define M0_PERH             0xe000e000
#define _SysTick            (M0_PERH + 0x10)
#define _NVIC               (M0_PERH + 0x100)
#define _SC                 (M0_PERH + 0xd00)

// TODO
#endif