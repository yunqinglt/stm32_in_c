#include "main.h"

#define _RCC                (_AHB1 + 0x3800)
#define RCC_AHB1ENR  (*(__IO uint32_t *)(_RCC + 0x30))

volatile uint32_t isr_hit = 0;

void My_SVC_Handler(void) {
    isr_hit = 1;
    while (1) {
        __asm__ volatile ("nop"); // 如果中断成功触发，使用GDB调试时将看到PC停在这个循环内
    }
}

int main(void) {
    
    _vector_to_ram();

    // 1. 替换 RAM 向量表中的 SVCall 异常处理地址 (索引 11)
    // Cortex-M 架构要求中断函数地址的最低位(LSB)必须为 1，以表示 Thumb 状态
    ram_vector_table[11] = (uint32_t)My_SVC_Handler | 1;

    // 2. 触发软中断 (SVCall)
    __asm__ volatile ("svc 0");
    // uint32_t ms10 = __get_tenms();
    // uint32_t exact = __is_tenms_exact();

    RCC_AHB1ENR |= (1 << 0);

    // __gpio_set_port_mode(GPIOA, GPIO_PORT_MODE_OUTPUT, 7);
    // __gpio_set_output_mode(GPIOA, GPIO_OUTPUT_PP, 7);
    // __gpio_set_output_speed(GPIOA, GPIO_SPEED_HIGH, 7);
    // __gpio_set_output_pull(GPIOA, GPIO_OUTPUT_PLDN, 7);
    // __gpio_set_output_value(GPIOA, 7);

    

    while (1) {
        __asm__ volatile ("mov r8, r8");
    }
}

