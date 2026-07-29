#include "main.h"

#define _RCC                (_AHB1 + 0x3800)
#define RCC_AHB1ENR  (*(__IO uint32_t *)(_RCC + 0x30))

int main(void) {
    
    // _vector_to_ram();
    // uint32_t ms10 = __get_tenms();
    // uint32_t exact = __is_tenms_exact();

    RCC_AHB1ENR |= (1 << 0);

    __gpio_set_port_mode(GPIOA, GPIO_PORT_MODE_OUTPUT, 7);
    __gpio_set_output_mode(GPIOA, GPIO_OUTPUT_PP, 7);
    __gpio_set_output_speed(GPIOA, GPIO_SPEED_HIGH, 7);
    __gpio_set_output_pull(GPIOA, GPIO_OUTPUT_PLDN, 7);
    __gpio_set_output_value(GPIOA, 7);
    
    while (1) {
        __asm__ volatile ("mov r8, r8");
    }
}

