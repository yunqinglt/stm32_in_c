#ifndef MIPSEL_EMU_PLATFORM_H
#define MIPSEL_EMU_PLATFORM_H

#include "config.h"
#include "registers.h"
#include "uart16550.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Compatibility name used by the desktop frontend and older callers. */
#define PLATFORM_MEMORY_SIZE MIPSEL_EMU_RAM_SIZE

/*
 * Raw physical-memory backend.  These callbacks never receive MMIO accesses;
 * they are suitable for a flat host buffer, cached SPI PSRAM, or another
 * byte-addressable storage provider.  Returning false reports a backend
 * failure.  platform_memory_configure() requires read and write; fill is
 * optional and has a portable write-based fallback.
 */
typedef bool (*platform_memory_read_fn)(void *opaque, uint32_t pa,
                                        void *dst, size_t len);
typedef bool (*platform_memory_write_fn)(void *opaque, uint32_t pa,
                                         const void *src, size_t len);
typedef bool (*platform_memory_fill_fn)(void *opaque, uint32_t pa,
                                        uint8_t value, size_t len);

typedef struct {
    platform_memory_read_fn read;
    platform_memory_write_fn write;
    platform_memory_fill_fn fill;
} platform_memory_ops_t;

bool platform_memory_configure(const platform_memory_ops_t *ops,
                               void *opaque, uint32_t size);
bool platform_memory_bind(uint8_t *bytes, uint32_t size);
uint32_t platform_memory_size(void);
bool platform_memory_read(uint32_t pa, void *dst, size_t len);
bool platform_memory_write(uint32_t pa, const void *src, size_t len);
bool platform_memory_fill(uint32_t pa, uint8_t value, size_t len);

void platform_init(uart16550_tx_callback_t uart_tx, void *opaque);
void platform_reset(void);
bool platform_uart_can_receive(void);
bool platform_uart_receive(uint8_t byte);

/*
 * Zero-copy USB/DMA bridge for the platform-owned UART instance.  RX reserve
 * exposes storage that a CDC OUT DMA transfer may fill; TX peek exposes bytes
 * for a CDC IN transfer.  Call produce/consume only after DMA completion and
 * serialize these calls with guest MMIO execution.
 */
size_t platform_uart_rx_reserve(uint8_t **bytes);
bool platform_uart_rx_produce(size_t length);
size_t platform_uart_tx_peek(const uint8_t **bytes);
bool platform_uart_tx_consume(size_t length);
size_t platform_uart_tx_count(void);

void platform_update_interrupts(Registers *state);

/*
 * Checked physical-bus access used by monitor/debug transports.  Width must be
 * 1, 2, or 4 bytes.  The bus includes MMIO, so reads may have device side
 * effects; a false result reports an unmapped address or backend failure.
 */
bool platform_bus_read(uint32_t addr, unsigned width, uint32_t *value);
bool platform_bus_write(uint32_t addr, unsigned width, uint32_t value);

/* The frontend supplies one byte-addressed RAM instance to platform.c. */
/*
 * Little-endian physical-memory accessors. Until bus-error propagation is
 * added, out-of-range reads return zero and out-of-range writes are ignored.
 */
uint8_t read8(uint32_t addr);
uint16_t read16(uint32_t addr);
uint32_t read32(uint32_t addr);

void write8(uint32_t addr, uint8_t data);
void write16(uint32_t addr, uint16_t data);
void write32(uint32_t addr, uint32_t data);

#endif
