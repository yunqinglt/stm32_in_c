#ifndef _RCC_H
#define _RCC_H

#include "stm32f042_memory.h"

typedef struct {
    __IO uint32_t CR[1];
    __IO uint32_t CFGR[1];

    __IO uint32_t CIR[1];
    __IO uint32_t APB2RSTR[1];
    __IO uint32_t APB1RSTR[1];

    __IO uint32_t AHBENR[1];
    __IO uint32_t APB2ENR[1];
    __IO uint32_t APB1ENR[1];

    __IO uint32_t BDCR[1];
    __IO uint32_t CSR[1];

    __IO uint32_t AHBRSTR[1];

    __IO uint32_t CFGR2[1];
    __IO uint32_t CFGR3[1];

    __IO uint32_t CR2[1];
} __PACKED rcc_t


#define RCC     ((rcc_t *) _RCC)



#endif