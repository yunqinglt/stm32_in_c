#ifndef MIPSEL_EMU_UART16550_H
#define MIPSEL_EMU_UART16550_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SEAD-3 compatible UART wiring. Registers are 32-bit, low byte only. */
#ifndef UART16550_MMIO_BASE
#define UART16550_MMIO_BASE       MIPSEL_EMU_UART_MMIO_BASE
#endif
#define UART16550_REG_SHIFT       2u
#define UART16550_REG_STRIDE      (1u << UART16550_REG_SHIFT)
#define UART16550_REGISTER_COUNT  8u
#define UART16550_MMIO_SIZE       (UART16550_REGISTER_COUNT * UART16550_REG_STRIDE)
#ifndef UART16550_CLOCK_HZ
#define UART16550_CLOCK_HZ        MIPSEL_EMU_UART_CLOCK_HZ
#endif
#ifndef UART16550_IRQ_LINE
#define UART16550_IRQ_LINE        MIPSEL_EMU_UART_IRQ_LINE
#endif
#ifndef UART16550_RX_FIFO_SIZE
#ifdef MIPSEL_EMU_UART_RX_FIFO_SIZE
#define UART16550_RX_FIFO_SIZE    MIPSEL_EMU_UART_RX_FIFO_SIZE
#else
#define UART16550_RX_FIFO_SIZE    16u
#endif
#endif

#ifndef UART16550_TX_FIFO_SIZE
#ifdef MIPSEL_EMU_UART_TX_FIFO_SIZE
#define UART16550_TX_FIFO_SIZE    MIPSEL_EMU_UART_TX_FIFO_SIZE
#else
#define UART16550_TX_FIFO_SIZE    16u
#endif
#endif

#if UART16550_RX_FIFO_SIZE < 1
#error "UART16550_RX_FIFO_SIZE must be at least one byte"
#endif

#if UART16550_TX_FIFO_SIZE < 1
#error "UART16550_TX_FIFO_SIZE must be at least one byte"
#endif

enum uart16550_register {
    UART16550_REG_RBR_THR_DLL = 0,
    UART16550_REG_IER_DLM     = 1,
    UART16550_REG_IIR_FCR     = 2,
    UART16550_REG_LCR         = 3,
    UART16550_REG_MCR         = 4,
    UART16550_REG_LSR         = 5,
    UART16550_REG_MSR         = 6,
    UART16550_REG_SCR         = 7,
};

enum uart16550_ier_bits {
    UART16550_IER_RX_AVAILABLE = 1u << 0,
    UART16550_IER_THR_EMPTY    = 1u << 1,
    UART16550_IER_LINE_STATUS  = 1u << 2,
    UART16550_IER_MODEM_STATUS = 1u << 3,
};

enum uart16550_lsr_bits {
    UART16550_LSR_DATA_READY = 1u << 0,
    UART16550_LSR_OVERRUN    = 1u << 1,
    UART16550_LSR_THR_EMPTY  = 1u << 5,
    UART16550_LSR_TX_EMPTY   = 1u << 6,
};

#define UART16550_LCR_DLAB       (1u << 7)
#define UART16550_FCR_ENABLE     (1u << 0)
#define UART16550_FCR_CLEAR_RX   (1u << 1)
#define UART16550_FCR_CLEAR_TX   (1u << 2)
#define UART16550_IIR_NO_PENDING (1u << 0)

typedef void (*uart16550_tx_callback_t)(void *opaque, uint8_t byte);

typedef struct uart16550 {
    uint8_t divisor_latch_low;
    uint8_t divisor_latch_high;
    uint8_t ier;
    uint8_t fcr;
    uint8_t lcr;
    uint8_t mcr;
    uint8_t scr;
    uint8_t line_status_errors;

    uint8_t rx_fifo[UART16550_RX_FIFO_SIZE];
    size_t rx_head;
    size_t rx_count;
    size_t rx_reserved;

    uint8_t tx_fifo[UART16550_TX_FIFO_SIZE];
    size_t tx_head;
    size_t tx_count;
    size_t tx_peeked;

    bool thre_irq_pending;
    uart16550_tx_callback_t tx_callback;
    void *tx_opaque;
} uart16550_t;

/* init() installs the host TX sink; reset() preserves that wiring. */
void uart16550_init(uart16550_t *uart,
                    uart16550_tx_callback_t tx_callback,
                    void *tx_opaque);
void uart16550_reset(uart16550_t *uart);
void uart16550_set_tx_callback(uart16550_t *uart,
                               uart16550_tx_callback_t tx_callback,
                               void *tx_opaque);

/*
 * width is in bytes and may be 1, 2, or 4. The device-tree contract uses
 * width 4; narrower accesses are accepted for diagnostics. Accesses to the
 * padding bytes between registers are handled as read-zero/write-ignore.
 */
bool uart16550_mmio_contains(uint32_t address, unsigned width);
bool uart16550_mmio_read(uart16550_t *uart, uint32_t address,
                         unsigned width, uint32_t *value);
bool uart16550_mmio_write(uart16550_t *uart, uint32_t address,
                          unsigned width, uint32_t value);

/* Host/TUI receive path. A rejected push records a hardware overrun. */
bool uart16550_rx_can_accept(const uart16550_t *uart);
bool uart16550_rx_push(uart16550_t *uart, uint8_t byte);
size_t uart16550_rx_push_bytes(uart16550_t *uart,
                               const uint8_t *bytes, size_t length);
size_t uart16550_rx_count(const uart16550_t *uart);

/*
 * Zero-copy host receive producer interface. reserve() returns the largest
 * contiguous writable span. After filling any prefix of that span, publish it
 * with produce(). There may be only one producer, and it must not mix this
 * interface with rx_push() while a DMA transfer is outstanding.
 *
 * A zero-length reservation is normal backpressure and does not record an
 * overrun. produce() rejects a length that was not reserved. Calling it with
 * zero cancels the reservation. FIFO reset invalidates outstanding spans, so
 * an actual DMA engine must be quiesced before resetting the UART.
 *
 * These functions form a single-producer/single-consumer protocol, but do not
 * provide interrupt locking. Serialize completion calls with guest MMIO
 * accesses (for example at an emulator instruction/time-slice boundary).
 */
size_t uart16550_rx_reserve(uart16550_t *uart, uint8_t **bytes);
bool uart16550_rx_produce(uart16550_t *uart, size_t length);

/*
 * Zero-copy host transmit consumer interface. peek() returns the largest
 * contiguous readable span. Once the host/DMA has copied a prefix, consume()
 * releases it. Emptying the FIFO asserts THRE and, when enabled, its IRQ.
 *
 * The callback API is retained for desktop compatibility and drains this
 * queue synchronously. Embedded users should initialize with a NULL callback
 * and consume the spans from their USB task instead. FIFO reset invalidates
 * an outstanding peek, and consume() with zero cancels it.
 */
bool uart16550_tx_can_accept(const uart16550_t *uart);
size_t uart16550_tx_count(const uart16550_t *uart);
size_t uart16550_tx_peek(uart16550_t *uart,
                         const uint8_t **bytes);
bool uart16550_tx_consume(uart16550_t *uart, size_t length);

/* Pure interrupt status helpers for driving the MIPS CPU interrupt line. */
uint8_t uart16550_iir(const uart16550_t *uart);
bool uart16550_irq_pending(const uart16550_t *uart);

#endif
