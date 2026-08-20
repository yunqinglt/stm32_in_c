#include "emu.h"
#include "exception.h"
#include "op.h"
#include "platform.h"
#include "registers.h"

#include <stdint.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

uint8_t *pool;
vmstate_t *status;

int main(void) {
    Registers state;
    vmstate_t vm = {0};
    uint32_t mtc0_compare;
    uint32_t mtc0_cause;

    pool = calloc(1, PLATFORM_MEMORY_SIZE);
    CHECK(pool != NULL);
    CHECK(platform_memory_bind(pool, PLATFORM_MEMORY_SIZE));
    status = &vm;
    reset_cpu(&state);
    platform_init(NULL, NULL);

    state.cp0.byname.cp0r9_t.cp0r9_n.Count = 0;
    state.cp0.byname.cp0r11_t.cp0r11_n.Compare = 1;
    update_cycle(&state);
    CHECK(GET_BITFIELD(state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                       CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN) == 1);
    CHECK((CAUSE_IP(&state) & (1u << 7)) != 0);

    /* Guest writes cannot clear timer or external hardware pending bits. */
    state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                     CP0_CAUSE_IP_POS + UART16550_IRQ_LINE, 1, 1);
    state.gpr[2] = UINT32_C(1) << CP0_CAUSE_IP_POS;
    mtc0_cause = (0x10u << 26) | (0x04u << 21) |
                 (2u << 16) | (13u << 11);
    op_mtc0(mtc0_cause, &state);
    CHECK(GET_BITFIELD(state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                       CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN) == 1);
    CHECK((CAUSE_IP(&state) & (1u << 7)) != 0);
    CHECK((CAUSE_IP(&state) & (1u << UART16550_IRQ_LINE)) != 0);
    CHECK((CAUSE_IP(&state) & 1u) != 0);

    /* mtc0 $v0, Compare clears the latched timer interrupt. */
    state.gpr[2] = 100;
    mtc0_compare = (0x10u << 26) | (0x04u << 21) |
                   (2u << 16) | (11u << 11);
    op_mtc0(mtc0_compare, &state);
    CHECK(state.cp0.byname.cp0r11_t.cp0r11_n.Compare == 100);
    CHECK(GET_BITFIELD(state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                       CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN) == 0);
    CHECK((CAUSE_IP(&state) & (1u << 7)) == 0);

    /* A pending, enabled IP7 is delivered between instructions. */
    state.pc = 0x80001000u;
    state.next_pc = state.pc + 4u;
    state.cp0.byname.cp0r12_t.cp0r12_n.Status =
        SET_BITFIELD(0, CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN, 1);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status =
        SET_BITFIELD(state.cp0.byname.cp0r12_t.cp0r12_n.Status,
                     CP0_STATUS_IM_POS + 7, 1, 1);
    state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                     CP0_CAUSE_IP_POS + 7, 1, 1);
    cpu_step(&state);
    CHECK(state.pc == 0x80000180u);
    CHECK(CAUSE_EXCCODE(&state) == EXC_INT);
    CHECK(STATUS_EXL(&state) == 1);

    free(pool);
    return 0;
}
