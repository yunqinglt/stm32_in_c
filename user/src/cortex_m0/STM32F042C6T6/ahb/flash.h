#ifndef _FLASH_H
#define _FLASH_H

#include "stm32f042_memory.h"

typedef struct {
    __IO uint32_t ACR[1];
    __IO uint32_t KEYR[1];
    __IO uint32_t OPTKEYR[1];

    __IO uint32_t SR[1];
    __IO uint32_t CR[1];
    __IO uint32_t AR[1];

    __IO uint32_t OBR[1];
    __IO uint32_t WRPR[1];
} __PACKED flash_t;

#define flash   ((flash_t *) FLASH_Base)


__STATIC_FORCEINLINE void __flash_enable_prefetch_buffer(uint32_t bit) {
    flash->ACR[0] |= (0x01 << 4);
}

__STATIC_FORCEINLINE void __flash_disable_prefetch_buffer(uint32_t bit) {
    flash->ACR[0] &= ~(0x01 << 4);
}

__STATIC_FORCEINLINE uint32_t __flash_get_prefetch_buffer(void) {
    return (flash->ACR[0] >> 5) & 0x01;
}


// 000 = Zero wait state, 001 = One wait state
__STATIC_FORCEINLINE void __flash_set_latency(uint32_t latency) {
    flash->ACR[0] = ((flash->ACR[0] & 0xfffffffc) | latency);
}

__STATIC_FORCEINLINE uint32_t __flash_get_latency(void) {
    return flash->ACR[0] & 0x03;
}




#endif