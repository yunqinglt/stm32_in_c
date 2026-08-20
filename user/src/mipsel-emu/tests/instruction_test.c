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

static uint32_t special(unsigned rs, unsigned rt, unsigned rd,
                        unsigned sa, unsigned function) {
    return (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | function;
}

static uint32_t special2(unsigned rs, unsigned rt, unsigned rd,
                         unsigned sa, unsigned function) {
    return (0x1cu << 26) | special(rs, rt, rd, sa, function);
}

static uint32_t special3(unsigned rs, unsigned rt, unsigned rd,
                         unsigned sa, unsigned function) {
    return (0x1fu << 26) | special(rs, rt, rd, sa, function);
}

static int executes_as_ri(uint32_t instr) {
    Registers state = {0};

    state.pc = UINT32_C(0x80000000);
    state.next_pc = state.pc + 4u;
    execute_instr(instr, &state);
    return state.exception_pending && CAUSE_EXCCODE(&state) == EXC_RI;
}

int main(void) {
    Registers state = {0};

    state.gpr[3] = UINT32_C(0x12345678);
    execute_instr(special(0, 3, 2, 8, 0x02), &state);
    CHECK(state.gpr[2] == UINT32_C(0x00123456));

    execute_instr(special(1, 3, 2, 8, 0x02), &state);
    CHECK(state.gpr[2] == UINT32_C(0x78123456));
    execute_instr(special(1, 3, 2, 0, 0x02), &state);
    CHECK(state.gpr[2] == UINT32_C(0x12345678));

    state.gpr[4] = 8;
    execute_instr(special(4, 3, 2, 0, 0x06), &state);
    CHECK(state.gpr[2] == UINT32_C(0x00123456));
    execute_instr(special(4, 3, 2, 1, 0x06), &state);
    CHECK(state.gpr[2] == UINT32_C(0x78123456));

    CHECK(executes_as_ri(special(2, 3, 2, 8, 0x02)));
    CHECK(executes_as_ri(special(4, 3, 2, 2, 0x06)));

    execute_instr((0x0fu << 26) | (2u << 16) | 0x8042u, &state);
    CHECK(state.gpr[2] == UINT32_C(0x80420000));

    state.gpr[3] = UINT32_C(0x12345680);
    execute_instr(special3(0, 3, 2, 0x10, 0x20), &state);
    CHECK(state.gpr[2] == UINT32_C(0xffffff80));
    CHECK(executes_as_ri(special3(1, 3, 2, 0x10, 0x20)));

    state.gpr[3] = UINT32_C(0x00f00000);
    /* Exact GCC 14/Linux encoding seen in the kernel: clz $v1, $v1. */
    execute_instr(UINT32_C(0x70631820), &state);
    CHECK(state.gpr[3] == 8u);
    state.gpr[3] = 0;
    execute_instr(special2(3, 2, 2, 0, 0x20), &state);
    CHECK(state.gpr[2] == 32u);
    state.gpr[3] = UINT32_C(0xff0fffff);
    execute_instr(special2(3, 2, 2, 0, 0x21), &state);
    CHECK(state.gpr[2] == 8u);
    state.gpr[3] = UINT32_MAX;
    execute_instr(special2(3, 2, 2, 0, 0x21), &state);
    CHECK(state.gpr[2] == 32u);
    CHECK(executes_as_ri(special2(3, 1, 2, 0, 0x20)));
    CHECK(executes_as_ri(special2(3, 2, 2, 1, 0x21)));

    state.cp0.byname.cp0r9_t.cp0r9_n.Count = UINT32_C(0x12345678);
    state.gpr[2] = 0;
    execute_instr(special3(0, 3, 2, 0, 0x3b), &state);
    CHECK(state.gpr[3] == UINT32_C(0x12345678));
    CHECK(executes_as_ri(special3(1, 3, 2, 0, 0x3b)));
    CHECK(executes_as_ri(special3(0, 3, 2, 1, 0x3b)));
    CHECK(executes_as_ri(special3(0, 3, 4, 0, 0x3b)));

    {
        Registers denied = {0};
        Registers enabled = {0};
        uint32_t rdhwr_count = special3(0, 3, 2, 0, 0x3b);

        denied.pc = UINT32_C(0x80000000);
        denied.next_pc = denied.pc + 4u;
        denied.cp0.byname.cp0r12_t.cp0r12_n.Status =
            2u << CP0_STATUS_KSU_POS;
        denied.cp0.byname.cp0r9_t.cp0r9_n.Count = UINT32_C(0x89abcdef);
        execute_instr(rdhwr_count, &denied);
        CHECK(denied.exception_pending &&
              CAUSE_EXCCODE(&denied) == EXC_RI);

        enabled.cp0.byname.cp0r12_t.cp0r12_n.Status =
            2u << CP0_STATUS_KSU_POS;
        enabled.cp0.byname.cp0r7_t.cp0r7_n.HWREna = UINT32_C(1) << 2;
        enabled.cp0.byname.cp0r9_t.cp0r9_n.Count = UINT32_C(0x89abcdef);
        execute_instr(rdhwr_count, &enabled);
        CHECK(!enabled.exception_pending);
        CHECK(enabled.gpr[3] == UINT32_C(0x89abcdef));
    }

    state.gpr[3] = UINT32_C(0xfffffffe);
    state.gpr[4] = 3;
    execute_instr(special(3, 4, 0, 0, 0x18), &state);
    CHECK(state.hi == UINT32_MAX);
    CHECK(state.lo == UINT32_C(0xfffffffa));
    CHECK(executes_as_ri(special(3, 4, 1, 0, 0x18)));
    CHECK(executes_as_ri(special(3, 4, 0, 1, 0x18)));

    state.gpr[3] = UINT32_MAX;
    state.gpr[4] = UINT32_MAX;
    execute_instr(special(3, 4, 0, 0, 0x19), &state);
    CHECK(state.hi == UINT32_C(0xfffffffe));
    CHECK(state.lo == UINT32_C(0x00000001));

    /* Reciprocal multiply used by the kernel's decimal formatting path. */
    state.gpr[3] = 101u;
    state.gpr[4] = UINT32_C(0x028f5c29);
    execute_instr(special(3, 4, 0, 0, 0x19), &state);
    CHECK(state.hi == UINT32_C(0x00000001));
    CHECK(state.lo == UINT32_C(0x028f5c2d));

    state.gpr[3] = UINT32_C(0xf0e1d2c3);
    execute_instr(special3(3, 2, 7, 8, 0x00), &state);
    CHECK(state.gpr[2] == UINT32_C(0xd2));
    execute_instr(special3(3, 2, 31, 0, 0x00), &state);
    CHECK(state.gpr[2] == UINT32_C(0xf0e1d2c3));

    state.gpr[2] = UINT32_C(0xaaaa5555);
    state.gpr[3] = UINT32_C(0x12);
    execute_instr(special3(3, 2, 15, 8, 0x04), &state);
    CHECK(state.gpr[2] == UINT32_C(0xaaaa1255));
    state.gpr[3] = UINT32_C(0xdead);
    execute_instr(special3(3, 2, 31, 16, 0x04), &state);
    CHECK(state.gpr[2] == UINT32_C(0xdead1255));

    state.gpr[3] = 7;
    execute_instr((0x09u << 26) | (3u << 21) | (2u << 16) | 0xffffu,
                  &state);
    CHECK(state.gpr[2] == 6u);
    state.gpr[3] = UINT32_MAX;
    execute_instr((0x09u << 26) | (3u << 21) | (2u << 16) | 1u,
                  &state);
    CHECK(state.gpr[2] == 0u);
    state.gpr[3] = 0;
    execute_instr((0x09u << 26) | (3u << 21) | (2u << 16) | 0xffffu,
                  &state);
    CHECK(state.gpr[2] == UINT32_MAX);

    state.exception_pending = 0;
    op_wfe(UINT32_C(0x42000020), &state);
    CHECK(!state.exception_pending);

    pool = calloc(1, PLATFORM_MEMORY_SIZE);
    CHECK(pool != NULL);
    CHECK(platform_memory_bind(pool, PLATFORM_MEMORY_SIZE));
    platform_init(NULL, NULL);
    write32(0x100u, UINT32_C(0x11223344));
    state.gpr[4] = UINT32_C(0x80000100);
    execute_instr((0x30u << 26) | (4u << 21) | (2u << 16), &state);
    CHECK(state.gpr[2] == UINT32_C(0x11223344));
    CHECK(state.ll_bit && state.ll_addr == 0x100u);
    state.gpr[2] = UINT32_C(0xaabbccdd);
    execute_instr((0x38u << 26) | (4u << 21) | (2u << 16), &state);
    CHECK(state.gpr[2] == 1u);
    CHECK(read32(0x100u) == UINT32_C(0xaabbccdd));
    CHECK(!state.ll_bit);

    state.gpr[2] = UINT32_C(0x55667788);
    execute_instr((0x38u << 26) | (4u << 21) | (2u << 16), &state);
    CHECK(state.gpr[2] == 0u);
    CHECK(read32(0x100u) == UINT32_C(0xaabbccdd));
    free(pool);
    return 0;
}
