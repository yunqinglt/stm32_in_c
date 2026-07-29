#ifndef _SCB_H
#define _SCB_H

#include "memory.h"


typedef struct {
    __IM uint32_t CPUID[1];
    __IO uint32_t ICSR[1];
    __reserved(0, 4);  // Cortex-M0 doesnt implement VTOR register

    __IO uint32_t AIRCR[1];
    __IO uint32_t SCR[1];
    __IM uint32_t CCR[1];
    __reserved(1, 4); // Cortex-M0 doesnt implenent SHPR1 register

    __IO uint8_t SHPR[8];
    __reserved(2, 104);
} __PACKED scb;

#define scb_t   ((scb *) _SC)

#define cpuid   scb_t->CPUID[0]
#define icsr    scb_t->ICSR[0]

#define aircr   scb_t->AIRCR[0]

#define scr     scb_t->SCR[0]
#define ccr     scb_t->CCR[0]

#define shpr(n)   scb_t->SHPR[n]


typedef enum {
    svcall = 3,
    pendsv = 6,
    systick = 7,
    reserved = 0,
} IRQ2_n;


__STATIC_FORCEINLINE const uint8_t __cpuid_imper(void) {
    return ((uint8_t) cpuid >> 24);
}

__STATIC_FORCEINLINE const uint8_t __cpuid_varcst(void) {
    return ((uint8_t) (cpuid & 0x00ff0000) >> 16);
}

__STATIC_FORCEINLINE const uint16_t __cpuid_partn(void) {
    return ((uint16_t) cpuid);
}



__WARNING("Caution: Triggering NMI manually is dangerous!!!")
__STATIC_FORCEINLINE void __trigger_nmi(void) {
    icsr |= (0x01 << 31);
}

__STATIC_FORCEINLINE uint32_t __is_nmi_pending(void) {
    return (uint32_t) ((icsr & (0x01 << 31)) ? 1 : 0);
}



__STATIC_FORCEINLINE void __trigger_pendsv(void) {
    icsr |= (0x01 << 28);
}

__STATIC_FORCEINLINE void __clear_pendsv_pending(void) {
    icsr &= ~(0x01 << 27);
}

__STATIC_FORCEINLINE uint32_t __is_pendsv_pending(void) {
    return (uint32_t) ((icsr & (0x01 << 28)) ? 1 : 0);
}



__STATIC_FORCEINLINE void __trigger_systick(void) {
    icsr |= (0x01 << 26);
}

__STATIC_FORCEINLINE void __clear_systick_pending(void) {
    icsr &= ~(0x01 << 25);
}

__STATIC_FORCEINLINE uint32_t __is_systick_pending(void) {
    return (uint32_t) ((icsr & (0x01 << 26)) ? 1 : 0);
}



__WARNING("This indication is excluding NMI and Faults, you should know that before using it.")
__STATIC_FORCEINLINE uint32_t __is_global_pending(void) {
    return (uint32_t) ((icsr & (0x01 << 22)) ? 1 : 0);
}

__WARNING("This indication just tells us the highest priority pending.")
__STATIC_FORCEINLINE uint8_t __get_current_pending(void) {
    return (uint8_t) ((icsr >> 12) & 0x3f);
}

__STATIC_FORCEINLINE uint8_t __get_current_handling(void) {
    return (uint8_t) ((icsr >> 0) & 0x3f);
}



__WARNING("!!!")
__STATIC_FORCEINLINE void __set_big_endian(void) {
    aircr = ((aircr & 0x0000ffff) | (1 << 15) | (0x05fa << 16));
}

__WARNING("!!!")
__STATIC_FORCEINLINE void __set_little_endian(void) {
    aircr = ((aircr & 0x00007fff) | (0x05fa << 16));
}

// Reqtest a reset event
__STATIC_FORCEINLINE void __system_reset(void) {
    __asm__ volatile ("dsb" :::);

    aircr = ((aircr & 0x0000ffff) | (1 << 2) | (0x05fa << 16));
    __asm__ volatile ("dsb" :::);

    while (1) __asm__ volatile ("nop");
}



__STATIC_FORCEINLINE void __set_event_pending(void) {
    scr |= (0x01 << 4);
}

__STATIC_FORCEINLINE void __clear_event_pending(void) {
    scr &= ~(0x01 << 4);
}

__STATIC_FORCEINLINE void __sleep(void) {
    scr &= ~(0x01 << 2);
}

__STATIC_FORCEINLINE void __deep_sleep(void) {
    scr |= (0x01 << 2);
}

__STATIC_FORCEINLINE void __not_sleep_on_exit(void) {
    scr &= ~(0x01 << 1);
}

__STATIC_FORCEINLINE void __sleep_on_exit(void) {
    scr |= (0x01 << 1);
}

__STATIC_FORCEINLINE uint32_t __get_ccr(void) {
    return ccr;
}

__STATIC_FORCEINLINE uint32_t __get_irq2_state(IRQ2_n irqn) {
    return (uint32_t) (shpr(irqn) >> 6);
}

__STATIC_FORCEINLINE void __set_irq2_state(IRQ2_n irqn, uint8_t pri) {
    shpr(irqn) = (uint8_t) ((pri & 0x03) << 6);
}

#endif