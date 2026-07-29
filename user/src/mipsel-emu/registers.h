#ifndef _REGISTER_H
#define _REGISTER_H

#include "compiler.h"
#include <stdint.h>

typedef enum {
    RUNNING,
    STEPPING,
    HALTING,
} vm_state;
typedef struct {
    vm_state state;
    union {
        uint32_t u_ud;
        void *p_ud;
    } userdata;
} vmstate_pointer;

typedef enum {
    USER,
    SUPERVISOR,
    KERNEL,
} CP0_Status;

typedef struct {
    uint32_t gpr[32];
    uint32_t pc;
    uint32_t cp0[32];
    

    struct {
        uint32_t entryhi;
        uint32_t entrylo0;
        uint32_t entrylo1;
        uint32_t pmask;
    } tlb[64];

    uint32_t hi;
    uint32_t lo;

} Registers;

#endif