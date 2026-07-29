#ifndef _STM32G474VET6_MEMORY_H
#define _STM32G474VET6_MEMORY_H

#ifndef _STM32_PERHA
#define _STM32_PERHA        0x40000000

#include "memory.h"

#define FSMC_BASE           0xa0000000
#define QSPI_BASE           0xa0001000

#define _AHB2               (_STM32_PERHA + 0x08000000)
#define _AHB1               (_STM32_PERHA + 0x00020000)
#define _APB2               (_STM32_PERHA + 0x00010000)
#define _APB1               (_STM32_PERHA)

#define TIM2_BASE           (_APB1 + 0x00000000)
#define TIM3_BASE           (_APB1 + 0x00000400)
#define TIM4_BASE           (_APB1 + 0x00000800)
#define TIM5_BASE           (_APB1 + 0x00000c00)
#define TIM6_BASE           (_APB1 + 0x00001000)
#define TIM7_BASE           (_APB1 + 0x00001400)

#define CRS_BASE            (_APB1 + 0x00002000)
#define TAMP_BASE           (_APB1 + 0x00002400)
#define RTCBKP_BASE         (_APB1 + 0x00002800)
#define WWDG_BASE           (_APB1 + 0x00002c00)
#define IWDG_BASE           (_APB1 + 0x00003000)
#define SPI2I2S2_BASE       (_APB1 + 0x00003800)
#define SPI3I2S3_BASE       (_APB1 + 0x00003c00)

#define USART2_BASE         (_APB1 + 0x00004400)
#define USART3_BASE         (_APB1 + 0x00004800)
#define UART4_BASE          (_APB1 + 0x00004c00)
#define UART5_BASE          (_APB1 + 0x00005000)
#define I2C1_BASE           (_APB1 + 0x00005400)
#define I2C2_BASE           (_APB1 + 0x00005800)
#define USBFS_BASE          (_APB1 + 0x00005c00)
#define USBSRAM_BASE        (_APB1 + 0x00006000)
#define FDCAN1_BASE         (_APB1 + 0x00006400)
#define FDCAN2_BASE         (_APB1 + 0x00006800)
#define FDCAN3_BASE         (_APB1 + 0x00006c00)
#define PWR_BASE            (_APB1 + 0x00007000)

#define I2C3_BASE           (_APB1 + 0x00007800)
#define LPTIM1_BASE         (_APB1 + 0x00007c00)
#define LPUART1_BASE        (_APB1 + 0x00008000)
#define I2C4_BASE           (_APB1 + 0x00008400)

#define UCPD1_BASE          (_APB1 + 0x0000a000)
#define FDCANM_BASE         (_APB1 + 0x0000a400) // *3






#endif
#endif