#ifndef _HANDLER_H
#define _HANDLER_H

#include "memory.h"
#include "startup.h"

#include "scb.h"

__REWRITE void NMI_Handler(void) {
    while (1) __asm__ volatile ("nop");
}

__REWRITE void HardFault_Handler(void) {
    __system_reset();
}

#endif