#include "uart16550.h"

#define UART16550_IIR_MODEM_STATUS 0x00u
#define UART16550_IIR_THR_EMPTY    0x02u
#define UART16550_IIR_RX_AVAILABLE 0x04u
#define UART16550_IIR_LINE_STATUS  0x06u
#define UART16550_IIR_ID_MASK      0x0eu
#define UART16550_IIR_FIFO_BITS    0xc0u

#define UART16550_FCR_DMA_MODE     (1u << 3)
#define UART16550_FCR_TRIGGER_MASK (3u << 6)
#define UART16550_FCR_STORED_MASK  \
    (UART16550_FCR_ENABLE | UART16550_FCR_DMA_MODE | \
     UART16550_FCR_TRIGGER_MASK)

#define UART16550_MCR_DTR   (1u << 0)
#define UART16550_MCR_RTS   (1u << 1)
#define UART16550_MCR_OUT1  (1u << 2)
#define UART16550_MCR_OUT2  (1u << 3)
#define UART16550_MCR_LOOP  (1u << 4)
#define UART16550_MCR_MASK  0x1fu

#define UART16550_MSR_CTS   (1u << 4)
#define UART16550_MSR_DSR   (1u << 5)
#define UART16550_MSR_RI    (1u << 6)
#define UART16550_MSR_DCD   (1u << 7)
#define UART16550_MSR_READY \
    (UART16550_MSR_CTS | UART16550_MSR_DSR | UART16550_MSR_DCD)

static void drain_tx_callback(uart16550_t *uart);

static bool access_width_supported(unsigned width) {
    return width == 1u || width == 2u || width == 4u;
}

bool uart16550_mmio_contains(uint32_t address, unsigned width) {
    uint32_t offset;

    if (!access_width_supported(width) || address < UART16550_MMIO_BASE) {
        return false;
    }

    offset = address - UART16550_MMIO_BASE;
    return offset <= UART16550_MMIO_SIZE - width;
}

static void clear_rx_fifo(uart16550_t *uart) {
    uart->rx_head = 0;
    uart->rx_count = 0;
    uart->rx_reserved = 0;
}

static void clear_tx_fifo(uart16550_t *uart) {
    uart->tx_head = 0;
    uart->tx_count = 0;
    uart->tx_peeked = 0;
}

void uart16550_reset(uart16550_t *uart) {
    if (!uart) {
        return;
    }

    uart->divisor_latch_low = 0;
    uart->divisor_latch_high = 0;
    uart->ier = 0;
    uart->fcr = 0;
    uart->lcr = 0;
    uart->mcr = 0;
    uart->scr = 0;
    uart->line_status_errors = 0;
    clear_rx_fifo(uart);
    clear_tx_fifo(uart);

    /* The transmitter is idle at reset, but its interrupt is masked. */
    uart->thre_irq_pending = true;
}

void uart16550_init(uart16550_t *uart,
                    uart16550_tx_callback_t tx_callback,
                    void *tx_opaque) {
    if (!uart) {
        return;
    }

    uart->tx_callback = tx_callback;
    uart->tx_opaque = tx_opaque;
    uart16550_reset(uart);
}

void uart16550_set_tx_callback(uart16550_t *uart,
                               uart16550_tx_callback_t tx_callback,
                               void *tx_opaque) {
    if (!uart) {
        return;
    }

    uart->tx_callback = tx_callback;
    uart->tx_opaque = tx_opaque;
    drain_tx_callback(uart);
}

static uint8_t line_status(const uart16550_t *uart) {
    uint8_t status = 0;

    if (uart->rx_count != 0) {
        status |= UART16550_LSR_DATA_READY;
    }
    if (uart->tx_count == 0) {
        status |= UART16550_LSR_THR_EMPTY | UART16550_LSR_TX_EMPTY;
    }

    return status | uart->line_status_errors;
}

static uint8_t modem_status(const uart16550_t *uart) {
    uint8_t status = 0;

    if (!(uart->mcr & UART16550_MCR_LOOP)) {
        return UART16550_MSR_READY;
    }

    if (uart->mcr & UART16550_MCR_RTS) {
        status |= UART16550_MSR_CTS;
    }
    if (uart->mcr & UART16550_MCR_DTR) {
        status |= UART16550_MSR_DSR;
    }
    if (uart->mcr & UART16550_MCR_OUT1) {
        status |= UART16550_MSR_RI;
    }
    if (uart->mcr & UART16550_MCR_OUT2) {
        status |= UART16550_MSR_DCD;
    }

    return status;
}

uint8_t uart16550_iir(const uart16550_t *uart) {
    uint8_t fifo_bits;
    uint8_t identification;

    if (!uart) {
        return UART16550_IIR_NO_PENDING;
    }

    fifo_bits = (uart->fcr & UART16550_FCR_ENABLE) ?
        UART16550_IIR_FIFO_BITS : 0;

    if ((uart->ier & UART16550_IER_LINE_STATUS) &&
        uart->line_status_errors) {
        identification = UART16550_IIR_LINE_STATUS;
    } else if ((uart->ier & UART16550_IER_RX_AVAILABLE) &&
               uart->rx_count != 0) {
        identification = UART16550_IIR_RX_AVAILABLE;
    } else if ((uart->ier & UART16550_IER_THR_EMPTY) &&
               uart->thre_irq_pending) {
        identification = UART16550_IIR_THR_EMPTY;
    } else {
        /* No modem-status delta inputs are modeled. */
        (void) UART16550_IIR_MODEM_STATUS;
        return fifo_bits | UART16550_IIR_NO_PENDING;
    }

    return fifo_bits | identification;
}

bool uart16550_irq_pending(const uart16550_t *uart) {
    return (uart16550_iir(uart) & UART16550_IIR_NO_PENDING) == 0;
}

static uint8_t pop_rx(uart16550_t *uart) {
    uint8_t byte;

    if (uart->rx_count == 0) {
        return 0;
    }

    byte = uart->rx_fifo[uart->rx_head];
    uart->rx_head = (uart->rx_head + 1u) % UART16550_RX_FIFO_SIZE;
    --uart->rx_count;
    return byte;
}

static size_t tx_capacity(const uart16550_t *uart) {
    return (uart->fcr & UART16550_FCR_ENABLE) ?
        UART16550_TX_FIFO_SIZE : 1u;
}

bool uart16550_tx_can_accept(const uart16550_t *uart) {
    return uart && uart->tx_count < tx_capacity(uart);
}

size_t uart16550_tx_count(const uart16550_t *uart) {
    return uart ? uart->tx_count : 0;
}

size_t uart16550_tx_peek(uart16550_t *uart,
                         const uint8_t **bytes) {
    size_t contiguous;

    if (!bytes) {
        return 0;
    }
    *bytes = NULL;
    if (!uart || uart->tx_count == 0) {
        return 0;
    }

    *bytes = &uart->tx_fifo[uart->tx_head];
    if (uart->tx_peeked != 0) {
        return uart->tx_peeked;
    }

    contiguous = UART16550_TX_FIFO_SIZE - uart->tx_head;
    uart->tx_peeked = uart->tx_count < contiguous ?
        uart->tx_count : contiguous;
    return uart->tx_peeked;
}

bool uart16550_tx_consume(uart16550_t *uart, size_t length) {
    if (!uart || length > uart->tx_peeked) {
        return false;
    }
    if (length == 0) {
        uart->tx_peeked = 0;
        return true;
    }

    uart->tx_head = (uart->tx_head + length) % UART16550_TX_FIFO_SIZE;
    uart->tx_count -= length;
    uart->tx_peeked = 0;
    if (uart->tx_count == 0) {
        uart->thre_irq_pending = true;
    }
    return true;
}

static void drain_tx_callback(uart16550_t *uart) {
    while (uart->tx_callback && uart->tx_count != 0) {
        const uint8_t *bytes;
        uint8_t byte;

        if (uart16550_tx_peek(uart, &bytes) == 0) {
            return;
        }
        byte = bytes[0];

        /* Release before invoking user code so a reentrant observer sees the
         * byte as transmitted and cannot consume it a second time. */
        (void) uart16550_tx_consume(uart, 1);
        uart->tx_callback(uart->tx_opaque, byte);
    }
}

static bool push_tx(uart16550_t *uart, uint8_t byte) {
    size_t tail;

    if (!uart16550_tx_can_accept(uart)) {
        return false;
    }

    tail = (uart->tx_head + uart->tx_count) % UART16550_TX_FIFO_SIZE;
    uart->tx_fifo[tail] = byte;
    ++uart->tx_count;
    uart->thre_irq_pending = false;
    drain_tx_callback(uart);
    return true;
}

static uint8_t read_register(uart16550_t *uart, unsigned reg) {
    uint8_t value;

    switch (reg) {
        case UART16550_REG_RBR_THR_DLL:
            if (uart->lcr & UART16550_LCR_DLAB) {
                return uart->divisor_latch_low;
            }
            return pop_rx(uart);

        case UART16550_REG_IER_DLM:
            if (uart->lcr & UART16550_LCR_DLAB) {
                return uart->divisor_latch_high;
            }
            return uart->ier;

        case UART16550_REG_IIR_FCR:
            value = uart16550_iir(uart);
            if ((value & UART16550_IIR_ID_MASK) ==
                UART16550_IIR_THR_EMPTY) {
                uart->thre_irq_pending = false;
            }
            return value;

        case UART16550_REG_LCR:
            return uart->lcr;

        case UART16550_REG_MCR:
            return uart->mcr;

        case UART16550_REG_LSR:
            value = line_status(uart);
            uart->line_status_errors = 0;
            return value;

        case UART16550_REG_MSR:
            return modem_status(uart);

        case UART16550_REG_SCR:
            return uart->scr;

        default:
            return 0;
    }
}

static void write_fcr(uart16550_t *uart, uint8_t value) {
    bool fifo_was_enabled = (uart->fcr & UART16550_FCR_ENABLE) != 0;
    bool fifo_is_enabled = (value & UART16550_FCR_ENABLE) != 0;

    if (fifo_was_enabled != fifo_is_enabled ||
        (value & UART16550_FCR_CLEAR_RX)) {
        clear_rx_fifo(uart);
        uart->line_status_errors = 0;
    }

    if (fifo_was_enabled != fifo_is_enabled ||
        (value & UART16550_FCR_CLEAR_TX)) {
        clear_tx_fifo(uart);
        uart->thre_irq_pending = true;
    }

    /* FIFO reset bits are self-clearing. */
    uart->fcr = value & UART16550_FCR_STORED_MASK;
}

static void write_register(uart16550_t *uart, unsigned reg, uint8_t value) {
    uint8_t old_ier;

    switch (reg) {
        case UART16550_REG_RBR_THR_DLL:
            if (uart->lcr & UART16550_LCR_DLAB) {
                uart->divisor_latch_low = value;
                return;
            }

            /* A full FIFO is backpressure: software that observes THRE will
             * wait until the host consumer has released the queued bytes. */
            (void) push_tx(uart, value);
            return;

        case UART16550_REG_IER_DLM:
            if (uart->lcr & UART16550_LCR_DLAB) {
                uart->divisor_latch_high = value;
                return;
            }

            old_ier = uart->ier;
            uart->ier = value & 0x0fu;
            if (!(old_ier & UART16550_IER_THR_EMPTY) &&
                (uart->ier & UART16550_IER_THR_EMPTY) &&
                uart->tx_count == 0) {
                uart->thre_irq_pending = true;
            }
            return;

        case UART16550_REG_IIR_FCR:
            write_fcr(uart, value);
            return;

        case UART16550_REG_LCR:
            uart->lcr = value;
            return;

        case UART16550_REG_MCR:
            uart->mcr = value & UART16550_MCR_MASK;
            return;

        case UART16550_REG_SCR:
            uart->scr = value;
            return;

        /* RBR, IIR, LSR, and MSR are read-only in their read mappings. */
        case UART16550_REG_LSR:
        case UART16550_REG_MSR:
        default:
            return;
    }
}

bool uart16550_mmio_read(uart16550_t *uart, uint32_t address,
                         unsigned width, uint32_t *value) {
    uint32_t offset;

    if (!uart || !value || !uart16550_mmio_contains(address, width)) {
        return false;
    }

    *value = 0;
    offset = address - UART16550_MMIO_BASE;

    if ((offset & (UART16550_REG_STRIDE - 1u)) == 0) {
        *value = read_register(uart, offset >> UART16550_REG_SHIFT);
    }

    return true;
}

bool uart16550_mmio_write(uart16550_t *uart, uint32_t address,
                          unsigned width, uint32_t value) {
    uint32_t offset;

    if (!uart || !uart16550_mmio_contains(address, width)) {
        return false;
    }

    offset = address - UART16550_MMIO_BASE;
    if ((offset & (UART16550_REG_STRIDE - 1u)) == 0) {
        write_register(uart, offset >> UART16550_REG_SHIFT,
                       (uint8_t) value);
    }

    return true;
}

bool uart16550_rx_can_accept(const uart16550_t *uart) {
    size_t capacity;

    if (!uart) return false;
    if (uart->rx_reserved != 0) return false;
    capacity = (uart->fcr & UART16550_FCR_ENABLE) ?
        UART16550_RX_FIFO_SIZE : 1u;
    return uart->rx_count < capacity;
}

size_t uart16550_rx_reserve(uart16550_t *uart, uint8_t **bytes) {
    size_t capacity;
    size_t available;
    size_t tail;
    size_t contiguous;

    if (!bytes) {
        return 0;
    }
    *bytes = NULL;
    if (!uart) {
        return 0;
    }
    if (uart->rx_reserved != 0) {
        return 0;
    }

    capacity = (uart->fcr & UART16550_FCR_ENABLE) ?
        UART16550_RX_FIFO_SIZE : 1u;
    available = capacity - uart->rx_count;
    if (available == 0) {
        return 0;
    }

    tail = (uart->rx_head + uart->rx_count) % UART16550_RX_FIFO_SIZE;
    contiguous = UART16550_RX_FIFO_SIZE - tail;
    if (contiguous > available) {
        contiguous = available;
    }
    *bytes = &uart->rx_fifo[tail];
    uart->rx_reserved = contiguous;
    return contiguous;
}

bool uart16550_rx_produce(uart16550_t *uart, size_t length) {
    if (!uart) {
        return false;
    }
    if (length == 0) {
        uart->rx_reserved = 0;
        return true;
    }

    if (length > uart->rx_reserved) {
        return false;
    }

    uart->rx_count += length;
    uart->rx_reserved = 0;
    return true;
}

bool uart16550_rx_push(uart16550_t *uart, uint8_t byte) {
    uint8_t *destination;

    if (!uart) {
        return false;
    }

    if (uart16550_rx_reserve(uart, &destination) == 0) {
        uart->line_status_errors |= UART16550_LSR_OVERRUN;
        return false;
    }

    *destination = byte;
    return uart16550_rx_produce(uart, 1);
}

size_t uart16550_rx_push_bytes(uart16550_t *uart,
                               const uint8_t *bytes, size_t length) {
    size_t accepted = 0;

    if (!uart || (!bytes && length != 0)) {
        return 0;
    }

    while (accepted < length && uart16550_rx_push(uart, bytes[accepted])) {
        ++accepted;
    }

    return accepted;
}

size_t uart16550_rx_count(const uart16550_t *uart) {
    return uart ? uart->rx_count : 0;
}
