#ifndef _MAIN_H
#define _MAIN_H


#include "compiler.h"
#include <stdint.h>
#include "memory.h"
#include "scb.h"
#include "stm32g474_memory.h"
// #include "ahb/gpio_register.h"

__attribute__((aligned(512))) __USED __attribute__((section(".ramvector")))
uint32_t ram_vector_table[86];
extern uint32_t g_pfnVectors[];

#ifdef _STM32G474VET6_MEMORY_H
__STATIC_FORCEINLINE void _vector_to_ram(void) {
    uint32_t *flash_vector = g_pfnVectors;

    for (uint8_t i = 0; i < 86; ++i) {
        ram_vector_table[i] = flash_vector[i];
    }

    __asm__ volatile ("dsb" :::);
    __asm__ volatile ("isb" :::);

    vtor = (uint32_t) ram_vector_table;

    __asm__ volatile ("dsb" :::);
    __asm__ volatile ("isb" :::);
}
#endif

#endif