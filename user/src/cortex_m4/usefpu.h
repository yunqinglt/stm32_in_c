#ifndef _USEFPU_H
#define _USEFPU_H

#include "memory.h"

typedef struct {
    __IO uint32_t CPACR[1];
} fpuacl_t;

typedef struct {
    __IO uint32_t FPCCR[1];
    __IO uint32_t FPACR[1];
    __IO uint32_t FPDSCR[1];
} __PACKED fpu_t;

#define fpuacl  ((fpuacl_t *) _FPUACL)
#define fpu     ((fpu_t *) _FPU)



#endif