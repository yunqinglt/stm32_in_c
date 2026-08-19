#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

extern uint8_t *pool;
static uart16550_t uart;
static bool platform_initialized;

// TODO
// readx(uint32_t addr, Registers *state)
// -> raise_exception(IBE)?
// -> raise_exception(MC)?

static bool address_range_valid(uint32_t addr, uint32_t width) {
    return width <= PLATFORM_MEMORY_SIZE &&
           addr <= PLATFORM_MEMORY_SIZE - width;
}

void platform_init(uart16550_tx_callback_t uart_tx, void *opaque) {
    uart16550_init(&uart, uart_tx, opaque);
    platform_initialized = true;
}

void platform_reset(void) {
    if (!platform_initialized) {
        uart16550_init(&uart, NULL, NULL);
        platform_initialized = true;
    } else {
        uart16550_reset(&uart);
    }
}

bool platform_uart_can_receive(void) {
    return uart16550_rx_can_accept(&uart);
}

bool platform_uart_receive(uint8_t byte) {
    return uart16550_rx_push(&uart, byte);
}

void platform_update_interrupts(Registers *state) {
    uint32_t cause;
    uint32_t pending;

    if (!state) return;
    cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
    pending = uart16550_irq_pending(&uart) ? 1u : 0u;
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(cause, CP0_CAUSE_IP_POS + UART16550_IRQ_LINE, 1,
                     pending);
}

uint8_t read8(uint32_t addr) {
    uint32_t value;
    if (uart16550_mmio_read(&uart, addr, 1, &value)) {
        return (uint8_t)value;
    }
    if (!address_range_valid(addr, 1)) return 0;

    return pool[addr];
}

uint16_t read16(uint32_t addr) {
    uint32_t value;
    if (uart16550_mmio_read(&uart, addr, 2, &value)) {
        return (uint16_t)value;
    }
    if (!address_range_valid(addr, 2)) return 0;

    return (uint16_t) pool[addr] |
           ((uint16_t) pool[addr + 1] << 8);
}

uint32_t read32(uint32_t addr) {
    uint32_t value;
    if (uart16550_mmio_read(&uart, addr, 4, &value)) return value;
    if (!address_range_valid(addr, 4)) return 0;

    return (uint32_t) pool[addr] |
           ((uint32_t) pool[addr + 1] << 8) |
           ((uint32_t) pool[addr + 2] << 16) |
           ((uint32_t) pool[addr + 3] << 24);
}

void write8(uint32_t addr, uint8_t data) {
    if (uart16550_mmio_write(&uart, addr, 1, data)) return;
    if (!address_range_valid(addr, 1)) return;

    pool[addr] = data;
}

void write16(uint32_t addr, uint16_t data) {
    if (uart16550_mmio_write(&uart, addr, 2, data)) return;
    if (!address_range_valid(addr, 2)) return;

    pool[addr] = (uint8_t) data;
    pool[addr + 1] = (uint8_t) (data >> 8);
}

void write32(uint32_t addr, uint32_t data) {
    if (uart16550_mmio_write(&uart, addr, 4, data)) return;
    if (!address_range_valid(addr, 4)) return;

    pool[addr] = (uint8_t) data;
    pool[addr + 1] = (uint8_t) (data >> 8);
    pool[addr + 2] = (uint8_t) (data >> 16);
    pool[addr + 3] = (uint8_t) (data >> 24);
}
