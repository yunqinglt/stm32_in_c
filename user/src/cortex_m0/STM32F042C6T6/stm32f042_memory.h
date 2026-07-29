#ifndef _STM32F042_MEMORY_H
#define _STM32F042_MEMORY_H

// STM32F042VET6
#ifndef _STM32_PERHA
#define _STM32_PERHA

#include "memory.h"

#define _STM32_PERH         0x40000000
#define _AHB2               (_STM32_PERH + 0x8000000)
#define _AHB1               (_STM32_PERH + 0x20000)
#define _APB                (_STM32_PERH + 0x0)


// AHB Bus
#define GPIOA_Base   (_AHB2 + 0)
#define GPIOB_Base   (_AHB2 + 0x400)
#define GPIOC_Base   (_AHB2 + 0x800)
#define GPIOD_Base   (_AHB2 + 0xc00)
#define GPIOE_Base   (_AHB2 + 0x1000)
#define GPIOF_Base   (_AHB2 + 0x1400)

#define DMA1_Base    (_AHB1 + 0x0)
#define DMA2_Base    (_AHB1 + 0x400)
#define RCC_Base     (_AHB1 + 0x1000)
#define FLASH_Base   (_AHB1 + 0x2000)
#define CRC_Base     (_AHB1 + 0x3000)
#define TSC_Base     (_AHB1 + 0x4000)


// APB Bus
#define TIM2_Base    (_APB + 0x0)
#define TIM3_Base    (_APB + 0x400)
#define TIM6_Base    (_APB + 0x1000)
#define TIM7_Base    (_APB + 0x1400)
#define TIM14_Base   (_APB + 0x2000)

#define RTC_Base     (_APB + 0x2800)
#define WWDG_Base    (_APB + 0x2c00)
#define IWDG_Base    (_APB + 0x3000)

#define SPI2_Base    (_APB + 0x3800)

#define USART2_Base  (_APB + 0x4400)
#define USART3_Base  (_APB + 0x4800)
#define USART4_Base  (_APB + 0x4c00)
#define USART5_Base  (_APB + 0x5000)

#define I2C1_Base    (_APB + 0x5400)
#define I2C2_Base    (_APB + 0x5800)
#define USB_Base     (_APB + 0x5c00)
#define USBCAN_Base  (_APB + 0x6000)
#define CAN_Base     (_APB + 0x6400)

#define CRS_Base     (_APB + 0x6c00)
#define PWR_Base     (_APB + 0x7000)
#define DAC_Base     (_APB + 0x7400)
#define CEC_Base     (_APB + 0x7800)

#define SYSCFG_COMP_Base    (_APB + 0x10000)
#define EXTI_Base    (_APB + 0x10400)

#define USART6_Base  (_APB + 0x11400)
#define USART7_Base  (_APB + 0x11800)
#define USART8_Base  (_APB + 0x11c00)
#define ADC_Base     (_APB + 0x12400)

#define TIM1_Base    (_APB + 0x12c00)
#define SPI1I2S1_Base      (_APB + 0x13000)

#define USART1_Base  (_APB + 0x13800)
#define TIM15_Base   (_APB + 0x14000)
#define TIM16_Base   (_APB + 0x14400)
#define TIM17_Base   (_APB + 0x14800)
#define DBGMCU_Base  (_APB + 0x15800)


#endif
#endif