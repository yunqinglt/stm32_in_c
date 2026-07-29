#ifndef _NVIC_H
#define _NVIC_H
#include "memory.h"


#ifndef __ASSEMBLER__

#include "compiler.h"
#include "startup.h"
#include <stdint.h>

#define CORTEX_M0

typedef struct {
    __IOM uint32_t ISER[1];        // ISER     NVIC+0
    __reserved(1, 0x7c);         // 对齐到 NVIC+0x80

    __IOM uint32_t ICER[1];        // ICER     NVIC+0x80
    __reserved(2, 0x17c);         // 对齐到 NVIC+0x200

    __IOM uint32_t ISPR[1];        // ISPR     NVIC+0x200
    __reserved(3, 0x7c);         // 对齐到 NVIC+0x280

    __IOM uint32_t ICPR[1];        // ICPR     NVIC+0x280
    __reserved(4, 0x17c);       // 对齐到 NVIC+0x400

    __IOM uint8_t IPR[32];         // IPR0-7
} __PACKED nvic_t;

#endif
#define nvic    ((nvic_t *) _NVIC) // 0xE000E100


#define iser      nvic->ISER[0]
#define icer      nvic->ICER[0]
#define ispr      nvic->ISPR[0]
#define icpr      nvic->ICPR[0]
#define pri(n)    nvic->IPR[n]



__STATIC_FORCEINLINE void __pending_an_irq(uint32_t irqn) {
    if (irqn < 32) {
        ispr = (0x01 << irqn);
    }
}

__STATIC_FORCEINLINE uint32_t __get_pending_irq(uint32_t irqn) {
    if (irqn < 32) {
        return (ispr & (0x01 << irqn) ? 1 : 0);
    } else return 0;
}



__STATIC_FORCEINLINE uint32_t __get_enabled_irq(uint32_t irqn) {
    if (irqn < 32) {
        return (iser & (0x01 << irqn) ? 1 : 0);
    } else return 0;
}

// before setting iser, you should ensure that the corresponding irqn isn't pending
__STATIC_FORCEINLINE void __set_irq_enabled(uint32_t irqn) {
    if (irqn < 32) {
        icpr = (0x01 << irqn);
        iser = (0x01 << irqn);
    }
}

__STATIC_FORCEINLINE void __clear_irq_enabled(uint32_t irqn) {
    if (irqn < 32) {
        icer = (0x01 << irqn);
    }
}



// pri 0..3, in Cortex-M0 only [7:6] will be implemented, and [5:0] will be regarded as zero or ignored
__STATIC_FORCEINLINE void __set_irq_priority(uint32_t irqn, uint8_t prior) {
    if (irqn < 32) {
        pri(irqn) = (uint8_t)((prior & 0x03) << 6);
    }
}

__STATIC_FORCEINLINE uint8_t __get_irq_priority(uint32_t irqn) {
    if (irqn < 32) {
        return (pri(irqn) >> 6);
    } else return 0;
}



#endif
