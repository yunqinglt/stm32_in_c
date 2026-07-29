#include "startup.h"

// CPU: Cortex-M0

extern void main(...);
extern void __libc_init_array(...);

__WEAK void Default_Handler(void) {
    while (1) ;
}

__ALIAS("Default_Handler") void NMI_Handler(void);
__ALIAS("Default_Handler") void HardFault_Handler(void);
__ALIAS("Default_Handler") void SVC_Handler(void);
__ALIAS("Default_Handler") void PendSV_Handler(void);
__ALIAS("Default_Handler") void SysTick_Handler(void);
__ALIAS("Default_Handler") void WWDG_IRQHandler(void);                   /* Window WatchDog              */
__ALIAS("Default_Handler") void PVD_VDDIO2_IRQHandler(void);             /* PVD and VDDIO2 through EXTI Line detect */
__ALIAS("Default_Handler") void RTC_IRQHandler(void);                    /* RTC through the EXTI line    */
__ALIAS("Default_Handler") void FLASH_IRQHandler(void);                  /* FLASH                        */
__ALIAS("Default_Handler") void RCC_CRS_IRQHandler(void);                /* RCC and CRS                  */
__ALIAS("Default_Handler") void EXTI0_1_IRQHandler(void);                /* EXTI Line 0 and 1            */
__ALIAS("Default_Handler") void EXTI2_3_IRQHandler(void);                /* EXTI Line 2 and 3            */
__ALIAS("Default_Handler") void EXTI4_15_IRQHandler(void);               /* EXTI Line 4 to 15            */
__ALIAS("Default_Handler") void TSC_IRQHandler(void);                    /* TSC                          */
__ALIAS("Default_Handler") void DMA1_Channel1_IRQHandler(void);          /* DMA1 Channel 1               */
__ALIAS("Default_Handler") void DMA1_Channel2_3_IRQHandler(void);        /* DMA1 Channel 2 and Channel 3 */
__ALIAS("Default_Handler") void DMA1_Channel4_5_6_7_IRQHandler(void);    /* DMA1 Channel 4, Channel 5, Channel 6 and Channel 7*/
__ALIAS("Default_Handler") void ADC1_COMP_IRQHandler(void);              /* ADC1, COMP1 and COMP2         */
__ALIAS("Default_Handler") void TIM1_BRK_UP_TRG_COM_IRQHandler(void);    /* TIM1 Break, Update, Trigger and Commutation */
__ALIAS("Default_Handler") void TIM1_CC_IRQHandler(void);                /* TIM1 Capture Compare         */
__ALIAS("Default_Handler") void TIM2_IRQHandler(void);                   /* TIM2                         */
__ALIAS("Default_Handler") void TIM3_IRQHandler(void);                   /* TIM3                         */
__ALIAS("Default_Handler") void TIM6_DAC_IRQHandler(void);               /* TIM6 and DAC                 */
__ALIAS("Default_Handler") void TIM7_IRQHandler(void);                   /* TIM7                         */
__ALIAS("Default_Handler") void TIM14_IRQHandler(void);                  /* TIM14                        */
__ALIAS("Default_Handler") void TIM15_IRQHandler(void);                  /* TIM15                        */
__ALIAS("Default_Handler") void TIM16_IRQHandler(void);                  /* TIM16                        */
__ALIAS("Default_Handler") void TIM17_IRQHandler(void);                  /* TIM17                        */
__ALIAS("Default_Handler") void I2C1_IRQHandler(void);                   /* I2C1                         */
__ALIAS("Default_Handler") void I2C2_IRQHandler(void);                   /* I2C2                         */
__ALIAS("Default_Handler") void SPI1_IRQHandler(void);                   /* SPI1                         */
__ALIAS("Default_Handler") void SPI2_IRQHandler(void);                   /* SPI2                         */
__ALIAS("Default_Handler") void USART1_IRQHandler(void);                 /* USART1                       */
__ALIAS("Default_Handler") void USART2_IRQHandler(void);                 /* USART2                       */
__ALIAS("Default_Handler") void USART3_4_IRQHandler(void);               /* USART3 and USART4            */
__ALIAS("Default_Handler") void CEC_CAN_IRQHandler(void);                /* CEC and CAN                  */
__ALIAS("Default_Handler") void USB_IRQHandler(void);                    /* USB  */


// startup_stm32f7.s:34-42
__STATIC_FORCEINLINE void LoopCopyDataInit(uint32_t *src, uint32_t *dest, uint32_t *data) {
    while (src < dest) {
        *src = *data;
        src++;
        data++;
    }
}

// startup_stm32f7.s:44-55
__STATIC_FORCEINLINE void LoopFillZerobss(uint32_t *start, uint32_t *end) {
    while (start < end) {
        *start = 0;
        start++;
    }
}

// Entry
__WEAK __NAKED void Reset_Handler(void) {
    
    __asm__ volatile (
        "mov sp, %0" 
        :
        : "r"(&_estack)
        : "sp"
    );

    SystemInit();
    LoopCopyDataInit(&_sdata, &_edata, &_sidata);
    LoopFillZerobss(&_sbss, &_ebss);

    // __libc_init_array();
    main(&_sdata);
}



__USED __ALIGN_4 __attribute__((section(".isr_vector")))
const uint32_t g_pfnVectors[] = {
    (uint32_t) &_estack,
    (uint32_t) &Reset_Handler,
    (uint32_t) &NMI_Handler,
    (uint32_t) &HardFault_Handler,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    (uint32_t) &SVC_Handler,
    0,
    0,
    (uint32_t) &PendSV_Handler,
    (uint32_t) &SysTick_Handler,
    (uint32_t) &WWDG_IRQHandler,
    (uint32_t) &PVD_VDDIO2_IRQHandler,
    (uint32_t) &RTC_IRQHandler,
    (uint32_t) &FLASH_IRQHandler,
    (uint32_t) &RCC_CRS_IRQHandler,
    (uint32_t) &EXTI0_1_IRQHandler,
    (uint32_t) &EXTI2_3_IRQHandler,
    (uint32_t) &EXTI4_15_IRQHandler,
    (uint32_t) &TSC_IRQHandler,
    (uint32_t) &DMA1_Channel1_IRQHandler,
    (uint32_t) &DMA1_Channel2_3_IRQHandler,
    (uint32_t) &DMA1_Channel4_5_6_7_IRQHandler,
    (uint32_t) &ADC1_COMP_IRQHandler,
    (uint32_t) &TIM1_BRK_UP_TRG_COM_IRQHandler,
    (uint32_t) &TIM1_CC_IRQHandler,
    (uint32_t) &TIM2_IRQHandler,
    (uint32_t) &TIM3_IRQHandler,
    (uint32_t) &TIM6_DAC_IRQHandler,
    (uint32_t) &TIM7_IRQHandler,
    (uint32_t) &TIM14_IRQHandler,
    (uint32_t) &TIM15_IRQHandler,
    (uint32_t) &TIM16_IRQHandler,
    (uint32_t) &TIM17_IRQHandler,
    (uint32_t) &I2C1_IRQHandler,
    (uint32_t) &I2C2_IRQHandler,
    (uint32_t) &SPI1_IRQHandler,
    (uint32_t) &SPI2_IRQHandler,
    (uint32_t) &USART1_IRQHandler,
    (uint32_t) &USART2_IRQHandler,
    (uint32_t) &USART3_4_IRQHandler,
    (uint32_t) &CEC_CAN_IRQHandler,
    (uint32_t) &USB_IRQHandler
};