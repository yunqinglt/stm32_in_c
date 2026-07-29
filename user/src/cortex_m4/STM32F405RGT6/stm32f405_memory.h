#ifndef _STM32F0405_MEMORY_H
#define _STM32F0405_MEMORY_H

// STM32F405RGT6
#ifndef _STM32_PERHA
#define _STM32_PERHA         0x40000000

#include "memory.h"

#define _AHB3               0xa0000000
#define _AHB2               (_STM32_PERHA + 0x10000000)
#define _AHB1               (_STM32_PERHA + 0x20000)
#define _APB2               (_STM32_PERHA + 0x10000)
#define _APB1               (_STM32_PERHA + 0x0)


// AHB Bus
#define GPIOA_Base   (_AHB1 + 0)
#define GPIOB_Base   (_AHB1 + 0x400)
#define GPIOC_Base   (_AHB1 + 0x800)
#define GPIOD_Base   (_AHB1 + 0xc00)
#define GPIOE_Base   (_AHB1 + 0x1000)
#define GPIOF_Base   (_AHB1 + 0x1400)
#define GPIOG_Base   (_AHB1 + 0x1800)
#define GPIOH_Base   (_AHB1 + 0x1c00)
#define GPIOI_Base   (_AHB1 + 0x2000)
#define GPIOJ_Base   (_AHB1 + 0x2400)
#define GPIOK_Base   (_AHB1 + 0x2800)

#define FLASH_Base   (_AHB1 + 0x3c00)
#define BKPSRAM_Base (_AHB1 + 0x4000)
#define DMA1_Base    (_AHB1 + 0x6000)
#define DMA2_Base    (_AHB1 + 0x6400)
#define ETHMAC_Base  (_AHB1 + 0x8000)

#define DMA2D_Base   (_AHB1 + 0xb000)
#define USBOTGFS1_Base   (_AHB1 + 0x20000)


#define USBOTGFS2_Base   (_AHB2 + 0x0)
#define DCMI_Base    (_AHB2 + 0x50000)
#define CRYP_Base    (_AHB2 + 0x60000)
#define HASH_Base    (_AHB2 + 0x60400)
#define RNG_Base     (_AHB2 + 0x60800)


#define FSMC_Base    (_AHB3 + 0x0)


// APB Bus
#define TIM2_Base    (_APB1 + 0x0)
#define TIM3_Base    (_APB1 + 0x400)
#define TIM4_Base    (_APB1 + 0x800)
#define TIM5_Base    (_APB1 + 0xc00)
#define TIM6_Base    (_APB1 + 0x1000)
#define TIM7_Base    (_APB1 + 0x1400)
#define TIM12_Base   (_APB1 + 0x1800)
#define TIM13_Base   (_APB1 + 0x1c00)
#define TIM14_Base   (_APB1 + 0x2000)

#define RTCBKP_Base  (_APB1 + 0x2800)
#define WWDG_Base    (_APB1 + 0x2c00)
#define IWDG_Base    (_APB1 + 0x3000)

#define I2S2ext_Base (_APB1 + 0x3400)
#define SPI2I2S2_Base    (_APB1 + 0x3800)
#define SPI3I2S3_Base    (_APB1 + 0x3c00)
#define I2S3ext_Base (_APB1 + 0x4000)

#define USART2_Base  (_APB1 + 0x4400)
#define USART3_Base  (_APB1 + 0x4800)
#define UART4_Base   (_APB1 + 0x4c00)
#define UART5_Base   (_APB1 + 0x5000)

#define I2C1_Base    (_APB1 + 0x5400)
#define I2C2_Base    (_APB1 + 0x5800)
#define USB_Base     (_APB1 + 0x5c00)

#define CAN1_Base    (_APB1 + 0x6400)
#define CAN2_Base    (_APB1 + 0x6800)

#define PWR_Base     (_APB1 + 0x7000)
#define DAC_Base     (_APB1 + 0x7400)
#define UART7_Base   (_APB1 + 0x7800)
#define UART8_Base   (_APB1 + 0x7c00)


#define TIM1_Base    (_APB2 + 0x0)
#define TIM8_Base    (_APB2 + 0x400)

#define USART1_Base  (_APB2 + 0x1000)
#define USART6_Base  (_APB2 + 0x1400)
#define ADC123_Base  (_APB2 + 0x2000)
#define SDIO_Base    (_APB2 + 0x2c00)
#define SPI1_Base    (_APB2 + 0x3000)
#define SPI4_Base    (_APB2 + 0x3400)

#define SYSCFG_Base  (_APB2 + 0x3800)
#define EXTI_Base    (_APB2 + 0x3c00)
#define TIM9_Base    (_APB2 + 0x4000)
#define TIM10_Base   (_APB2 + 0x4400)
#define TIM11_Base   (_APB2 + 0x4800)

#define SPI5_Base    (_APB2 + 0x5000)
#define SPI6_Base    (_APB2 + 0x5400)
#define SAI1_Base    (_APB2 + 0x5800)
#define LCDTFT_Base  (_APB2 + 0x6800)


#endif
#endif