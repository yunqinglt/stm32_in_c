#ifndef _GPIO_REGISTER_H
#define _GPIO_REGISTER_H

#include "stm32f042_memory.h"

typedef struct {
    __IO uint32_t MODER[1];

    __IO uint32_t OTYPER[1];

    __IO uint32_t OSPEEDR[1]; // Output Speed Register
    __IO uint32_t PUPDR[1];

    __IO uint32_t IDR[1];

    __IO uint32_t ODR[1]; // Output data register

    __IO uint32_t BSRR[1]; // Bit set-reset Register

    // A, B only
    __IO uint32_t LCKR[1];

    __IO uint32_t AFRL[1];
    __IO uint32_t AFRH[1];

    // Important!
    __IO uint32_t BRR[1];
} __PACKED gpio_t;

#define GPIOA   ((gpio_t *) GPIOA_Base)
#define GPIOB   ((gpio_t *) GPIOB_Base)
#define GPIOC   ((gpio_t *) GPIOC_Base)
#define GPIOD   ((gpio_t *) GPIOD_Base)
#define GPIOE   ((gpio_t *) GPIOE_Base)
#define GPIOF   ((gpio_t *) GPIOF_Base)

// 00 = Input mode, 01 = GPO mode, 10 = Alternate function mode, 11 = Analog mode
__STATIC_FORCEINLINE void __gpio_set_port_mode(gpio_t *GPIO, uint32_t mode, uint32_t pinx) {
    GPIO->MODER[0] = ((GPIO->MODER[0] & ~(0x03 << pinx * 2)) | (mode << pinx * 2));
}

__STATIC_FORCEINLINE uint32_t __gpio_get_port_mode(gpio_t *GPIO, uint32_t pinx) {
    return ((GPIO->MODER[0] >> pinx * 2) & 0x03);
}


// 0 = push-pull, 1 = open-drain
__STATIC_FORCEINLINE void __gpio_set_output_mode(gpio_t *GPIO, uint32_t mode, uint32_t pinx) {
    GPIO->OTYPER[0] = ((GPIO->OTYPER[0] & ~(0x01 << pinx)) | (mode << pinx));
}

__STATIC_FORCEINLINE uint32_t __gpio_get_output_mode(gpio_t *GPIO, uint32_t pinx) {
    return (GPIO->OTYPER[0] & (0x01 << pinx));
}


// x0 = Low speed, 01 = Medium speed, 11 = High speed
__STATIC_FORCEINLINE void __gpio_set_output_speed(gpio_t *GPIO, uint32_t speed, uint32_t pinx) {
    GPIO->OSPEEDR[0] = ((GPIO->OSPEEDR[0] & ~(0x03 << pinx * 2)) | (speed << pinx * 2));
}

__STATIC_FORCEINLINE uint32_t __gpio_get_output_speed(gpio_t *GPIO, uint32_t pinx) {
    return ((GPIO->OSPEEDR[0] >> pinx * 2) & 0x03);
}

// 00 = No pull-up, pull-down, 01 = Pull-up, 10 = Pull down, 11 = Reserved
__STATIC_FORCEINLINE void __gpio_set_output_pull(gpio_t *GPIO, uint32_t mode, uint32_t pinx) {
    GPIO->PUPDR[0] = ((GPIO->PUPDR[0] & ~(0x03 << pinx * 2)) | (mode << pinx * 2));
}

__STATIC_FORCEINLINE uint32_t __gpio_get_output_pull(gpio_t *GPIO, uint32_t pinx) {
    return ((GPIO->PUPDR[0] >> pinx * 2) & 0x03);
}


__STATIC_FORCEINLINE uint32_t __gpio_get_input_value(gpio_t *GPIO, uint32_t pinx) {
    return ((GPIO->IDR[0] >> pinx) & 0x01);
}


// !!!
__STATIC_FORCEINLINE void __gpio_setall_output_value(gpio_t *GPIO, uint32_t pinx_mask) {
    uint32_t primask;
    __asm__ volatile(
        "mrs %0, primask\n"
        "cpsid i"
        : "=r"(primask)
        :
        : "memory"
    );

    GPIO->ODR[0] |= (uint16_t) pinx_mask;

    __asm__ volatile ("isb");
    __asm__ volatile (
        "msr primask, %0"
        :
        : "r"(primask)
        : "memory"
    );
}

__STATIC_FORCEINLINE void __gpio_resetall_output_value(gpio_t *GPIO, uint32_t pinx_mask) {
    uint32_t primask;
    __asm__ volatile(
        "mrs %0, primask\n"
        "cpsid i"
        : "=r"(primask)
        :
        : "memory"
    );

    GPIO->ODR[0] &= ~(uint16_t) pinx_mask;

    __asm__ volatile ("isb");
    __asm__ volatile (
        "msr primask, %0"
        :
        : "r"(primask)
        : "memory"
    );
}

__STATIC_FORCEINLINE uint32_t __gpio_get_output_value(gpio_t *GPIO, uint32_t pinx) {
    return (GPIO->ODR[0] >> pinx) & 0x01;
}


__STATIC_FORCEINLINE void __gpio_set_output_value(gpio_t *GPIO, uint32_t pinx) {
    GPIO->BSRR[0] = (0x01 << pinx);
}

__STATIC_FORCEINLINE void __gpio_reset_output_value(gpio_t *GPIO, uint32_t pinx) {
    GPIO->BSRR[0] = (0x01 << (pinx + 0x10));
}


// GPIOA, B only
__WARNING("LCKR cant be recovered until System Reset")
__STATIC_FORCEINLINE void __gpio_set_pin_lock(gpio_t *GPIO, uint32_t pinmask) {
    uint32_t primask;
    __asm__ volatile(
        "mrs %0, primask\n"
        "cpsid i"
        : "=r"(primask)
        :
        : "memory"
    );

    volatile uint32_t t = 0x10000 | pinmask;

    GPIO->LCKR[0] = t;
    GPIO->LCKR[0] = pinmask;
    GPIO->LCKR[0] = t;

    t = GPIO->LCKR[0];
    t = GPIO->LCKR[0];

    __asm__ volatile ("isb");
    __asm__ volatile (
        "msr primask, %0"
        :
        : "r"(primask)
        : "memory"
    );
}

__STATIC_FORCEINLINE uint32_t __gpio_get_lckr_state(gpio_t *GPIO) {
    return (GPIO->LCKR[0] & 0x10000 ? 1 : 0);
}

// Return pin mask only
__STATIC_FORCEINLINE uint32_t __gpio_get_pin_locked(gpio_t *GPIO) {
    return (GPIO->LCKR[0] & 0xffff);
}


// AF: 0000 = AF0, 0001 = AF1..., 0111 = AF7, 1xxx = Reserved
__STATIC_FORCEINLINE void __gpio_set_pin_afl(gpio_t *GPIO, uint32_t af, uint32_t pinx) {
    GPIO->AFRL[0] = ((GPIO->AFRL[0] & ~(af << (pinx * 7))) | (af << pinx * 7));
}

__STATIC_FORCEINLINE void __gpio_set_pin_afh(gpio_t *GPIO, uint32_t af, uint32_t pinx) {
    GPIO->AFRH[0] = ((GPIO->AFRH[0] & ~(af << (pinx * 7))) | (af << pinx * 7));
}

__STATIC_FORCEINLINE uint32_t __gpio_get_pin_afl(gpio_t *GPIO, uint32_t pinx) {
    return ((GPIO->AFRL[0] >> pinx) & 0x01);
}

__STATIC_FORCEINLINE uint32_t __gpio_get_pin_afh(gpio_t *GPIO, uint32_t pinx) {
    return ((GPIO->AFRH[0] >> pinx) & 0x01);
}


__STATIC_FORCEINLINE void __gpio_write_brr(gpio_t *GPIO, uint32_t pinx) {
    GPIO->BRR[0] = (0x01 << pinx);
}

#endif