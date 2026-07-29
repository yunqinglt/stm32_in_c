#ifndef _STARTUP_H
#define _STARTUP_H

#include "compiler.h"
#include "memory.h"
#include <stdint.h>
// #include "main.h"

extern void Default_Handler(void);
extern void Reset_Handler(void);

__USED __STATIC_FORCEINLINE void SystemInit(void) {
    ;
}

#endif