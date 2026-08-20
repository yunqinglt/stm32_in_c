#include "uart16550.h"

#include <stddef.h>
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
#define REG_ADDRESS(reg) \
    (UART16550_MMIO_BASE + ((uint32_t) (reg) << UART16550_REG_SHIFT))

typedef struct tx_capture {
    uint8_t bytes[8];
    size_t count;
} tx_capture_t;

static void capture_tx(void *opaque, uint8_t byte) {
    tx_capture_t *capture = opaque;

    if (capture->count < sizeof(capture->bytes)) {
        capture->bytes[capture->count++] = byte;
    }
}

static uint32_t read_reg(uart16550_t *uart, unsigned reg) {
    uint32_t value = 0xffffffffu;

    if (!uart16550_mmio_read(uart, REG_ADDRESS(reg), 4, &value)) {
        return 0xffffffffu;
    }
    return value;
}

static int test_reset_and_decode(void) {
    uart16550_t uart;
    uint32_t value;

    uart16550_init(&uart, NULL, NULL);

    CHECK(UART16550_MMIO_SIZE == 0x20u);
    CHECK(uart16550_mmio_contains(UART16550_MMIO_BASE, 4));
    CHECK(uart16550_mmio_contains(UART16550_MMIO_BASE + 0x1cu, 4));
    CHECK(!uart16550_mmio_contains(UART16550_MMIO_BASE - 1u, 1));
    CHECK(!uart16550_mmio_contains(UART16550_MMIO_BASE + 0x20u, 1));
    CHECK(!uart16550_mmio_contains(UART16550_MMIO_BASE, 3));

    CHECK(read_reg(&uart, UART16550_REG_LSR) == 0x60u);
    CHECK(read_reg(&uart, UART16550_REG_IIR_FCR) == 0x01u);
    CHECK(read_reg(&uart, UART16550_REG_MSR) == 0xb0u);
    CHECK(!uart16550_irq_pending(&uart));

    value = 0xffu;
    CHECK(uart16550_mmio_read(&uart, UART16550_MMIO_BASE + 1u,
                              1, &value));
    CHECK(value == 0);
    CHECK(uart16550_mmio_write(&uart, UART16550_MMIO_BASE + 1u,
                               1, 0xffu));
    return 0;
}

static int test_dlab_and_transmit(void) {
    uart16550_t uart;
    tx_capture_t capture = {{0}, 0};

    uart16550_init(&uart, capture_tx, &capture);

    CHECK(uart16550_mmio_write(&uart, REG_ADDRESS(UART16550_REG_LCR),
                               4, UART16550_LCR_DLAB));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 8u));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IER_DLM),
                               4, 1u));
    CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == 8u);
    CHECK(read_reg(&uart, UART16550_REG_IER_DLM) == 1u);
    CHECK(capture.count == 0);

    CHECK(uart16550_mmio_write(&uart, REG_ADDRESS(UART16550_REG_LCR),
                               4, 3u));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 'A'));
    CHECK(capture.count == 1 && capture.bytes[0] == 'A');

    uart16550_reset(&uart);
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 'B'));
    CHECK(capture.count == 2 && capture.bytes[1] == 'B');
    return 0;
}

static int test_registers_and_tx_interrupt(void) {
    uart16550_t uart;
    const uint8_t *bytes;
    uint32_t iir;

    uart16550_init(&uart, NULL, NULL);

    CHECK(uart16550_mmio_write(&uart, REG_ADDRESS(UART16550_REG_MCR),
                               4, 0x1fu));
    CHECK(read_reg(&uart, UART16550_REG_MCR) == 0x1fu);
    CHECK(read_reg(&uart, UART16550_REG_MSR) == 0xf0u);
    CHECK(uart16550_mmio_write(&uart, REG_ADDRESS(UART16550_REG_SCR),
                               4, 0xa5u));
    CHECK(read_reg(&uart, UART16550_REG_SCR) == 0xa5u);

    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IER_DLM),
                               4, UART16550_IER_THR_EMPTY));
    CHECK(uart16550_irq_pending(&uart));
    CHECK(uart16550_iir(&uart) == 0x02u);

    CHECK(uart16550_mmio_read(&uart,
                              REG_ADDRESS(UART16550_REG_IIR_FCR),
                              4, &iir));
    CHECK(iir == 0x02u);
    CHECK(!uart16550_irq_pending(&uart));

    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 'X'));
    CHECK(!uart16550_irq_pending(&uart));
    CHECK(read_reg(&uart, UART16550_REG_LSR) == 0u);
    CHECK(uart16550_tx_count(&uart) == 1);
    CHECK(uart16550_tx_peek(&uart, &bytes) == 1);
    CHECK(bytes && bytes[0] == 'X');
    CHECK(uart16550_tx_consume(&uart, 1));
    CHECK(uart16550_irq_pending(&uart));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IER_DLM),
                               4, 0u));
    CHECK(!uart16550_irq_pending(&uart));
    return 0;
}

static int test_transmit_fifo_and_dma(void) {
    uart16550_t uart;
    tx_capture_t capture = {{0}, 0};
    const uint8_t *bytes;
    size_t length;
    size_t i;

    uart16550_init(&uart, NULL, NULL);

    /* A non-FIFO 16550 has one THR slot. Ignoring THRE must not overwrite it. */
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 'a'));
    CHECK(!uart16550_tx_can_accept(&uart));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 'b'));
    CHECK(uart16550_tx_count(&uart) == 1);
    CHECK(!uart16550_tx_consume(&uart, 1));
    CHECK(uart16550_tx_peek(&uart, &bytes) == 1);
    CHECK(bytes[0] == 'a');
    CHECK(!uart16550_tx_consume(&uart, 2));
    CHECK(uart16550_tx_consume(&uart, 1));
    CHECK(read_reg(&uart, UART16550_REG_LSR) == 0x60u);

    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IIR_FCR),
                               4, UART16550_FCR_ENABLE));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IER_DLM),
                               4, UART16550_IER_THR_EMPTY));
    CHECK(read_reg(&uart, UART16550_REG_IIR_FCR) == 0xc2u);

    for (i = 0; i < UART16550_TX_FIFO_SIZE; ++i) {
        CHECK(uart16550_mmio_write(
            &uart, REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
            4, (uint32_t) i));
    }
    CHECK(uart16550_tx_count(&uart) == UART16550_TX_FIFO_SIZE);
    CHECK(!uart16550_tx_can_accept(&uart));
    CHECK(read_reg(&uart, UART16550_REG_LSR) == 0u);
    CHECK(!uart16550_irq_pending(&uart));

    length = uart16550_tx_peek(&uart, &bytes);
    CHECK(length == UART16550_TX_FIFO_SIZE);
    for (i = 0; i < length; ++i) {
        CHECK(bytes[i] == (uint8_t) i);
    }
    CHECK(!uart16550_tx_consume(&uart, length + 1));
    CHECK(uart16550_tx_consume(&uart, 7));

    for (i = 0; i < 7; ++i) {
        CHECK(uart16550_mmio_write(
            &uart, REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
            4, (uint32_t) (UART16550_TX_FIFO_SIZE + i)));
    }
    CHECK(uart16550_tx_count(&uart) == UART16550_TX_FIFO_SIZE);

    length = uart16550_tx_peek(&uart, &bytes);
    CHECK(length == UART16550_TX_FIFO_SIZE - 7);
    for (i = 0; i < length; ++i) {
        CHECK(bytes[i] == (uint8_t) (i + 7));
    }
    CHECK(uart16550_tx_consume(&uart, length));
    CHECK(!uart16550_irq_pending(&uart));

    length = uart16550_tx_peek(&uart, &bytes);
    CHECK(length == 7);
    for (i = 0; i < length; ++i) {
        CHECK(bytes[i] == (uint8_t) (UART16550_TX_FIFO_SIZE + i));
    }
    CHECK(uart16550_tx_consume(&uart, length));
    CHECK(uart16550_tx_count(&uart) == 0);
    CHECK(uart16550_irq_pending(&uart));
    CHECK(read_reg(&uart, UART16550_REG_IIR_FCR) == 0xc2u);
    CHECK(!uart16550_irq_pending(&uart));

    /* Installing the legacy sink is a deliberate mode switch and drains any
     * bytes already queued by the embedded/DMA mode. */
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_RBR_THR_DLL),
                               4, 'z'));
    CHECK(uart16550_tx_count(&uart) == 1);
    uart16550_set_tx_callback(&uart, capture_tx, &capture);
    CHECK(uart16550_tx_count(&uart) == 0);
    CHECK(capture.count == 1 && capture.bytes[0] == 'z');
    return 0;
}

static int test_receive_fifo_and_interrupts(void) {
    uart16550_t uart;
    uint8_t input[UART16550_RX_FIFO_SIZE];
    uint32_t lsr;
    size_t i;

    uart16550_init(&uart, NULL, NULL);

    CHECK(uart16550_rx_can_accept(&uart));
    CHECK(uart16550_rx_push(&uart, 'a'));
    CHECK(!uart16550_rx_can_accept(&uart));
    CHECK(!uart16550_rx_push(&uart, 'b'));
    CHECK(read_reg(&uart, UART16550_REG_LSR) == 0x63u);
    CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == 'a');
    CHECK(read_reg(&uart, UART16550_REG_LSR) == 0x60u);

    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IIR_FCR),
                               4, UART16550_FCR_ENABLE));
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IER_DLM),
                               4, UART16550_IER_RX_AVAILABLE |
                                  UART16550_IER_LINE_STATUS));

    CHECK(uart16550_rx_push_bytes(&uart,
                                  (const uint8_t *) "abc", 3) == 3);
    CHECK(uart16550_rx_count(&uart) == 3);
    CHECK(uart16550_irq_pending(&uart));
    CHECK(uart16550_iir(&uart) == 0xc4u);
    CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == 'a');
    CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == 'b');
    CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == 'c');
    CHECK(!uart16550_irq_pending(&uart));

    for (i = 0; i < sizeof(input); ++i) {
        input[i] = (uint8_t) i;
    }
    CHECK(uart16550_rx_push_bytes(&uart, input, sizeof(input)) ==
          sizeof(input));
    CHECK(!uart16550_rx_can_accept(&uart));
    CHECK(!uart16550_rx_push(&uart, 0xffu));
    CHECK(uart16550_iir(&uart) == 0xc6u);
    CHECK(uart16550_mmio_read(&uart, REG_ADDRESS(UART16550_REG_LSR),
                              4, &lsr));
    CHECK((lsr & UART16550_LSR_OVERRUN) != 0);
    CHECK(uart16550_iir(&uart) == 0xc4u);

    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IIR_FCR),
                               4, UART16550_FCR_ENABLE |
                                  UART16550_FCR_CLEAR_RX));
    CHECK(uart16550_rx_count(&uart) == 0);
    CHECK(!uart16550_irq_pending(&uart));
    return 0;
}

static int test_receive_dma_spans(void) {
    uart16550_t uart;
    uint8_t *bytes;
    uint8_t *first_bytes;
    size_t length;
    size_t i;

    uart16550_init(&uart, NULL, NULL);
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IIR_FCR),
                               4, UART16550_FCR_ENABLE));

    length = uart16550_rx_reserve(&uart, &bytes);
    CHECK(length == UART16550_RX_FIFO_SIZE);
    CHECK(bytes != NULL);
    first_bytes = bytes;
    CHECK(!uart16550_rx_can_accept(&uart));
    CHECK(uart16550_rx_reserve(&uart, &bytes) == 0);
    CHECK(bytes == NULL);
    for (i = 0; i < 10; ++i) {
        first_bytes[i] = (uint8_t) i;
    }
    CHECK(!uart16550_rx_produce(&uart, length + 1));
    CHECK(uart16550_rx_produce(&uart, 10));
    CHECK(uart16550_rx_count(&uart) == 10);
    CHECK(!uart16550_rx_produce(&uart, 1));

    for (i = 0; i < 7; ++i) {
        CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == i);
    }

    length = uart16550_rx_reserve(&uart, &bytes);
    CHECK(length == UART16550_RX_FIFO_SIZE - 10);
    for (i = 0; i < length; ++i) {
        bytes[i] = (uint8_t) (10 + i);
    }
    CHECK(uart16550_rx_produce(&uart, length));

    length = uart16550_rx_reserve(&uart, &bytes);
    CHECK(length == 7);
    for (i = 0; i < length; ++i) {
        bytes[i] = (uint8_t) (UART16550_RX_FIFO_SIZE + i);
    }
    CHECK(uart16550_rx_produce(&uart, length));
    CHECK(uart16550_rx_count(&uart) == UART16550_RX_FIFO_SIZE);

    for (i = 7; i < UART16550_RX_FIFO_SIZE + 7; ++i) {
        CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == i);
    }
    CHECK(uart16550_rx_count(&uart) == 0);

    /* Resetting the FIFO invalidates an outstanding DMA reservation. */
    CHECK(uart16550_rx_reserve(&uart, &bytes) != 0);
    CHECK(uart16550_mmio_write(&uart,
                               REG_ADDRESS(UART16550_REG_IIR_FCR),
                               4, 0));
    CHECK(!uart16550_rx_produce(&uart, 1));
    CHECK(uart16550_rx_reserve(&uart, &bytes) == 1);
    bytes[0] = 'q';
    CHECK(uart16550_rx_produce(&uart, 1));
    CHECK(read_reg(&uart, UART16550_REG_RBR_THR_DLL) == 'q');
    return 0;
}

int main(void) {
    int result;

    result = test_reset_and_decode();
    if (result) return result;
    result = test_dlab_and_transmit();
    if (result) return result;
    result = test_registers_and_tx_interrupt();
    if (result) return result;
    result = test_transmit_fifo_and_dma();
    if (result) return result;
    result = test_receive_fifo_and_interrupts();
    if (result) return result;
    return test_receive_dma_spans();
}
