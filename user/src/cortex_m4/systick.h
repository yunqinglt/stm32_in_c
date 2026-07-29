#ifndef _SYSTICK_H
#define _SYSTICK_H

#include "memory.h"

typedef struct {
    __IO uint32_t CSR[1];
    __IO uint32_t RVR[1];
    __IO uint32_t CVR[1];
    __IM uint32_t CALIB[1];
} systick_t;

#define systick     ((systick_t *) _SysTick)

#define csr     systick->CSR[0]
#define rvr     systick->RVR[0]
#define cvr     systick->CVR[0]
#define calib   systick->CALIB[0]



__STATIC_FORCEINLINE void __systick_enable(void) {
    csr |= 0x01;
}

__STATIC_FORCEINLINE void __systick_disable(void) {
    csr &= (uint32_t) ~(0x01);
}

__STATIC_FORCEINLINE void __systick_expt_enable(void) {
    csr |= (0x01 << 1);
}

__STATIC_FORCEINLINE void __systick_expt_disable(void) {
    csr &= (uint32_t) ~(0x01 << 1);
}

__STATIC_FORCEINLINE void __select_ref_clock(void) {
    csr &= (uint32_t) ~(0x01 << 2);
}

__STATIC_FORCEINLINE void __select_pcs_clock(void) {
    csr |= (0x01 << 2);
}

__STATIC_FORCEINLINE uint32_t __counter_flag(void) {
    return (uint32_t) ((csr & (0x01 << 16)) ? 1 : 0 );
}



__STATIC_FORCEINLINE void __set_reload(uint32_t vpt) {
    rvr = (vpt & 0x00ffffff);
}

__STATIC_FORCEINLINE uint32_t __current_reload(void) {
    return cvr;
}



__STATIC_FORCEINLINE uint32_t __is_ref_clock(void) {
    return (uint32_t) ((calib & (0x01 << 31)) ? 0 : 1);
}

__STATIC_FORCEINLINE uint32_t __is_tenms_exact(void) {
    return (uint32_t) ((calib & (0x01 << 30)) ? 0 : 1);
}

__STATIC_FORCEINLINE uint32_t __get_tenms(void) {
    return (calib & 0x00ffffff);
}



#endif