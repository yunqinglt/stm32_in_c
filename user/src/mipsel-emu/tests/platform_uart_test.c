#include "emu.h"
#include "exception.h"
#include "platform.h"
#include "registers.h"

#include <stdint.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

uint8_t *pool;
vmstate_t *status;

static uint8_t transmitted[8];
static unsigned int transmitted_count;

static void capture_tx(void *opaque, uint8_t byte) {
    (void)opaque;
    if (transmitted_count < sizeof(transmitted)) {
        transmitted[transmitted_count++] = byte;
    }
}

int main(void) {
    Registers state = {0};

    pool = calloc(1, PLATFORM_MEMORY_SIZE);
    CHECK(pool != NULL);
    platform_init(capture_tx, NULL);

    write32(0x100u, 0x44332211u);
    CHECK(read8(0x100u) == 0x11u);
    CHECK(read16(0x101u) == 0x3322u);
    CHECK(read32(0x100u) == 0x44332211u);

    write32(UART16550_MMIO_BASE, 'A');
    CHECK(transmitted_count == 1u && transmitted[0] == 'A');
    CHECK(read32(UART16550_MMIO_BASE +
                 (UART16550_REG_LSR << UART16550_REG_SHIFT)) == 0x60u);

    /* A guest KSEG1 store reaches the UART through address translation. */
    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.pc = 0x80001000u;
    state.next_pc = state.pc + 4u;
    state.gpr[8] = 'B';
    state.gpr[9] = 0xbf000900u;
    write32(0x1000u, (0x2bu << 26) | (9u << 21) | (8u << 16));
    cpu_step(&state);
    CHECK(transmitted_count == 2u && transmitted[1] == 'B');

    /* Enable received-data interrupts and inject one host byte. */
    write32(UART16550_MMIO_BASE +
            (UART16550_REG_IER_DLM << UART16550_REG_SHIFT),
            UART16550_IER_RX_AVAILABLE);
    CHECK(platform_uart_can_receive());
    CHECK(platform_uart_receive('z'));
    CHECK(!platform_uart_can_receive());
    state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                     CP0_CAUSE_IP_POS + 7, 1, 1);
    platform_update_interrupts(&state);
    CHECK((CAUSE_IP(&state) & (1u << UART16550_IRQ_LINE)) != 0);
    CHECK((CAUSE_IP(&state) & (1u << 7)) != 0);
    CHECK(read32(UART16550_MMIO_BASE) == 'z');
    platform_update_interrupts(&state);
    CHECK((CAUSE_IP(&state) & (1u << UART16550_IRQ_LINE)) == 0);
    CHECK((CAUSE_IP(&state) & (1u << 7)) != 0);

    /* An enabled UART IP4 reaches the normal MIPS interrupt vector. */
    reset_cpu(&state);
    state.pc = 0x80001000u;
    state.next_pc = state.pc + 4u;
    state.cp0.byname.cp0r12_t.cp0r12_n.Status =
        SET_BITFIELD(0, CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN, 1);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status =
        SET_BITFIELD(state.cp0.byname.cp0r12_t.cp0r12_n.Status,
                     CP0_STATUS_IM_POS + UART16550_IRQ_LINE, 1, 1);
    CHECK(platform_uart_receive('i'));
    cpu_step(&state);
    CHECK(state.pc == 0x80000180u);
    CHECK(CAUSE_EXCCODE(&state) == EXC_INT);
    CHECK(STATUS_EXL(&state) == 1);

    platform_reset();
    CHECK(read32(UART16550_MMIO_BASE +
                 (UART16550_REG_IIR_FCR << UART16550_REG_SHIFT)) == 0x01u);

    free(pool);
    return 0;
}
