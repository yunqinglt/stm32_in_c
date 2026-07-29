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
#define M4_PERH             0xe000e000
#define _SysTick            (M4_PERH + 0x10)
#define _NVIC               (M4_PERH + 0x100)
#define _SC                 (M4_PERH + 0xd00)
#define _FPUACL             (M4_PERH + 0xd88)
#define _MPU                (M4_PERH + 0xd90)

#define _FPU                (M4_PERH + 0xf30)

// TODO
#endif