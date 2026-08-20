#include "platform.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if MIPSEL_EMU_ENABLE_UART16550
static uart16550_t uart;
#endif
static bool platform_initialized;

typedef struct {
    platform_memory_ops_t ops;
    void *opaque;
    uint32_t size;
} platform_memory_backend_t;

static platform_memory_backend_t memory_backend;

// TODO
// readx(uint32_t addr, Registers *state)
// -> raise_exception(IBE)?
// -> raise_exception(MC)?

static bool address_range_valid(uint32_t addr, size_t width) {
    return width <= memory_backend.size &&
           addr <= memory_backend.size - width;
}

static bool flat_memory_read(void *opaque, uint32_t pa, void *dst,
                             size_t len) {
    const uint8_t *bytes = opaque;

    if ((!bytes && len != 0) || (!dst && len != 0)) return false;
    if (len != 0) memcpy(dst, bytes + pa, len);
    return true;
}

static bool flat_memory_write(void *opaque, uint32_t pa, const void *src,
                              size_t len) {
    uint8_t *bytes = opaque;

    if ((!bytes && len != 0) || (!src && len != 0)) return false;
    if (len != 0) memcpy(bytes + pa, src, len);
    return true;
}

static bool flat_memory_fill(void *opaque, uint32_t pa, uint8_t value,
                             size_t len) {
    uint8_t *bytes = opaque;

    if (!bytes && len != 0) return false;
    if (len != 0) memset(bytes + pa, value, len);
    return true;
}

bool platform_memory_configure(const platform_memory_ops_t *ops,
                               void *opaque, uint32_t size) {
    if (!ops || !ops->read || !ops->write || size == 0) return false;

    memory_backend.ops = *ops;
    memory_backend.opaque = opaque;
    memory_backend.size = size;
    return true;
}

bool platform_memory_bind(uint8_t *bytes, uint32_t size) {
    static const platform_memory_ops_t flat_ops = {
        .read = flat_memory_read,
        .write = flat_memory_write,
        .fill = flat_memory_fill,
    };

    if (!bytes || size == 0) return false;
    return platform_memory_configure(&flat_ops, bytes, size);
}

uint32_t platform_memory_size(void) {
    return memory_backend.size;
}

bool platform_memory_read(uint32_t pa, void *dst, size_t len) {
    if ((!dst && len != 0) || !memory_backend.ops.read ||
        !address_range_valid(pa, len)) {
        return false;
    }
    if (len == 0) return true;
    return memory_backend.ops.read(memory_backend.opaque, pa, dst, len);
}

bool platform_memory_write(uint32_t pa, const void *src, size_t len) {
    if ((!src && len != 0) || !memory_backend.ops.write ||
        !address_range_valid(pa, len)) {
        return false;
    }
    if (len == 0) return true;
    return memory_backend.ops.write(memory_backend.opaque, pa, src, len);
}

bool platform_memory_fill(uint32_t pa, uint8_t value, size_t len) {
    static const size_t fill_chunk_size = 32u;
    uint8_t fill_chunk[32];

    if (!memory_backend.ops.write || !address_range_valid(pa, len)) {
        return false;
    }
    if (len == 0) return true;
    if (memory_backend.ops.fill) {
        return memory_backend.ops.fill(memory_backend.opaque, pa, value, len);
    }

    memset(fill_chunk, value, sizeof(fill_chunk));
    while (len != 0) {
        size_t amount = len < fill_chunk_size ? len : fill_chunk_size;
        if (!memory_backend.ops.write(memory_backend.opaque, pa,
                                      fill_chunk, amount)) {
            return false;
        }
        pa += (uint32_t)amount;
        len -= amount;
    }
    return true;
}

void platform_init(uart16550_tx_callback_t uart_tx, void *opaque) {
#if MIPSEL_EMU_ENABLE_UART16550
    uart16550_init(&uart, uart_tx, opaque);
#else
    (void)uart_tx;
    (void)opaque;
#endif
    platform_initialized = true;
}

void platform_reset(void) {
#if MIPSEL_EMU_ENABLE_UART16550
    if (!platform_initialized) {
        uart16550_init(&uart, NULL, NULL);
        platform_initialized = true;
    } else {
        uart16550_reset(&uart);
    }
#else
    platform_initialized = true;
#endif
}

bool platform_uart_can_receive(void) {
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_rx_can_accept(&uart);
#else
    return false;
#endif
}

bool platform_uart_receive(uint8_t byte) {
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_rx_push(&uart, byte);
#else
    (void)byte;
    return false;
#endif
}

size_t platform_uart_rx_reserve(uint8_t **bytes) {
    if (!bytes) return 0;
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_rx_reserve(&uart, bytes);
#else
    *bytes = NULL;
    return 0;
#endif
}

bool platform_uart_rx_produce(size_t length) {
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_rx_produce(&uart, length);
#else
    return length == 0;
#endif
}

size_t platform_uart_tx_peek(const uint8_t **bytes) {
    if (!bytes) return 0;
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_tx_peek(&uart, bytes);
#else
    *bytes = NULL;
    return 0;
#endif
}

bool platform_uart_tx_consume(size_t length) {
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_tx_consume(&uart, length);
#else
    return length == 0;
#endif
}

size_t platform_uart_tx_count(void) {
#if MIPSEL_EMU_ENABLE_UART16550
    return uart16550_tx_count(&uart);
#else
    return 0;
#endif
}

void platform_update_interrupts(Registers *state) {
#if MIPSEL_EMU_ENABLE_UART16550
    uint32_t cause;
    uint32_t pending;

    if (!state) return;
    cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
    pending = uart16550_irq_pending(&uart) ? 1u : 0u;
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(cause, CP0_CAUSE_IP_POS + UART16550_IRQ_LINE, 1,
                     pending);
#else
    (void)state;
#endif
}

bool platform_bus_read(uint32_t addr, unsigned width, uint32_t *value) {
#if MIPSEL_EMU_ENABLE_UART16550
    uint32_t mmio_value;
#endif
    uint8_t bytes[4] = {0};

    if (!value || (width != 1u && width != 2u && width != 4u)) return false;
#if MIPSEL_EMU_ENABLE_UART16550
    if (uart16550_mmio_read(&uart, addr, width, &mmio_value)) {
        *value = mmio_value;
        return true;
    }
#endif
    if (!platform_memory_read(addr, bytes, width)) return false;
    *value = (uint32_t)bytes[0];
    if (width >= 2u) *value |= (uint32_t)bytes[1] << 8;
    if (width == 4u) {
        *value |= (uint32_t)bytes[2] << 16;
        *value |= (uint32_t)bytes[3] << 24;
    }
    return true;
}

bool platform_bus_write(uint32_t addr, unsigned width, uint32_t value) {
    uint8_t bytes[4];

    if (width != 1u && width != 2u && width != 4u) return false;
#if MIPSEL_EMU_ENABLE_UART16550
    if (uart16550_mmio_write(&uart, addr, width, value)) return true;
#endif
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
    return platform_memory_write(addr, bytes, width);
}

uint8_t read8(uint32_t addr) {
    uint32_t value = 0;
    (void)platform_bus_read(addr, 1, &value);
    return (uint8_t)value;
}

uint16_t read16(uint32_t addr) {
    uint32_t value = 0;
    (void)platform_bus_read(addr, 2, &value);
    return (uint16_t)value;
}

uint32_t read32(uint32_t addr) {
    uint32_t value = 0;
    (void)platform_bus_read(addr, 4, &value);
    return value;
}

void write8(uint32_t addr, uint8_t data) {
    (void)platform_bus_write(addr, 1, data);
}

void write16(uint32_t addr, uint16_t data) {
    (void)platform_bus_write(addr, 2, data);
}

void write32(uint32_t addr, uint32_t data) {
    (void)platform_bus_write(addr, 4, data);
}
