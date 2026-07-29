#ifndef _PY32E407_MEMORY_H
#define _PY32E407_MEMORY_H

#ifndef _STM32_PERHA
#define _STM32_PERHA

#include "memory.h"


#define _PY32_PERH          0x40000000
#define _AHB3               0xa0001000 // Difference to STM32
#define _AHB2               (_PY32_PERH + 0x08000000)
#define _AHB1               (_PY32_PERH + 0x20000)
#define _APB2               (_PY32_PERH + 0x10000)
#define _APB1               (_PY32_PERH + 0x0)


// AHB Bus
#define ESMC_Base    (_AHB3 + 0x0)


#define GPIOA_Base   (_AHB2 + 0)
#define GPIOB_Base   (_AHB2 + 0x400)
#define GPIOC_Base   (_AHB2 + 0x800)
#define GPIOD_Base   (_AHB2 + 0xc00)
#define GPIOE_Base   (_AHB2 + 0x1000)
#define GPIOF_Base   (_AHB2 + 0x1400)
#define SDIO_Base    (_AHB2 + 0x2000)
#define AES_Base     (_AHB2 + 0x2400)
#define USB1OTG_Base (_AHB2 + 0x40000)
#define USB2OTG_Base (_AHB2 + 0x80000)
#define ETH_Base     (_AHB2 + 0xc0000)


#define DMA1_Base    (_AHB1 + 0x0)
#define DMA2_Base    (_AHB1 + 0x400)
#define CORDIC_Base  (_AHB1 + 0xc00)
#define RCC_Base     (_AHB1 + 0x1000)
#define FMC_Base     (_AHB1 + 0x2000)
#define CRC_Base     (_AHB1 + 0x3000)


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
#define TIM18_Base   (_APB1 + 0x2400)

#define RTC_Base     (_APB1 + 0x2800)
#define WWDG_Base    (_APB1 + 0x2c00)
#define IWDG_Base    (_APB1 + 0x3000)

#define SPI2I2S2_Base    (_APB1 + 0x3800)
#define SPI3I2S3_Base    (_APB1 + 0x3c00)

#define USART2_Base  (_APB1 + 0x4400)
#define USART3_Base  (_APB1 + 0x4800)
#define UART1_Base   (_APB1 + 0x4c00)
#define UART2_Base   (_APB1 + 0x5000)

#define I2C1_Base    (_APB1 + 0x5400)
#define I2C2_Base    (_APB1 + 0x5800)
#define UART3_Base   (_APB1 + 0x5c00)

#define BKP_Base     (_APB1 + 0x6c00) // 0x100

#define PWR_Base     (_APB1 + 0x7000)
#define DAC_Base     (_APB1 + 0x7400)
#define I2C3_Base    (_APB1 + 0x7800)
#define LPTIM1_Base  (_APB1 + 0x7c00)
#define LPUART1_Base (_APB1 + 0x8000)
#define I2C4_Base    (_APB1 + 0x8400)
#define CANMEM_Base  (_APB1 + 0xac00) // 0x800

#define CTC_Base     (_APB1 + 0xc800)
#define CANFD_Base   (_APB1 + 0xcc00)
#define CAN2P0_Base  (_APB1 + 0xe000)


#define SYSCFG_Base  (_APB2 + 0x0)
#define EXTI_Base    (_APB2 + 0x400)
#define ADC1_Base    (_APB2 + 0x2400)
#define ADC2_Base    (_APB2 + 0x2800)
#define TIM1_Base    (_APB2 + 0x2c00)
#define SPI1I2S1_Base  (_APB2 + 0x3000)
#define TIM8_Base    (_APB2 + 0x3400)

#define USART1_Base  (_APB2 + 0x3800)
#define ADC3_Base    (_APB2 + 0x3c00)

#define TIM9_Base    (_APB2 + 0x4c00)
#define TIM10_Base   (_APB2 + 0x5000)
#define TIM11_Base   (_APB2 + 0x5400)
#define TIM15_Base   (_APB2 + 0x5800)
#define TIM16_Base   (_APB2 + 0x5c00)
#define TIM17_Base   (_APB2 + 0x6000)
#define TIM19_Base   (_APB2 + 0x6400)

#define RNG_Base     (_APB2 + 0x6800)
#define COMP_Base    (_APB2 + 0x6c00)
#define OPA_Base     (_APB2 + 0x7000)
#define LCDC_Base    (_APB2 + 0x7400)

#endif
#endif