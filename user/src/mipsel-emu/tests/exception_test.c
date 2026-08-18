#include "exception.h"
#include "registers.h"
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static int test_cold_reset(void) {
    Registers state;

    reset_cpu(&state);

    CHECK(state.pc == MIPS_RESET_VECTOR);
    CHECK(state.next_pc == MIPS_RESET_VECTOR + 4u);
    CHECK(state.cp0.byname.cp0r12_t.cp0r12_n.Status == INIT_STATUS);
    CHECK(state.cp0.byname.cp0r1_t.cp0r1_n.Random == 63u);
    CHECK(state.cp0.byname.cp0r6_t.cp0r6_n.Wired == 0u);
    CHECK(state.cp0.byname.cp0r15_t.cp0r15_n.EBase == MIPS_DEFAULT_EBASE);
    CHECK(state.cp0.byname.cp0r16_t.cp0r16_n.Config == INIT_CONFIG0_R2);
    CHECK(state.cp0.byname.cp0r16_t.cp0r16_n.Config1 == INIT_CONFIG1);
    CHECK(!state.exception_pending && !state.bds && !state.ll_bit);
    return 0;
}

static int test_reset_exception_scheduling(void) {
    Registers state;

    reset_cpu(&state);
    state.pc = 0x80000000u;
    state.gpr[1] = 1;
    raise_exception(&state, 0, EXC_RESET, MIPS_VECTOR_RESET);

    CHECK(state.pc == MIPS_RESET_VECTOR);
    CHECK(state.next_pc == MIPS_RESET_VECTOR);
    CHECK(state.exception_pending);
    CHECK(state.gpr[1] == 0);

    reset_cpu(&state);
    state.pc = 0x80001000u;
    state.gpr[1] = 1;
    raise_exception(&state, 0, EXC_SRES, MIPS_VECTOR_RESET);

    CHECK(state.pc == MIPS_RESET_VECTOR);
    CHECK(state.next_pc == MIPS_RESET_VECTOR);
    CHECK(state.exception_pending);
    CHECK(state.gpr[1] == 1);
    CHECK(state.cp0.byname.cp0r30_t.cp0r30_n.ErrorEPC == 0x80001000u);
    return 0;
}

static int test_precise_exception(void) {
    Registers state;

    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.pc = 0x80001000u;

    raise_exception(&state, 0, EXC_SC, MIPS_VECTOR_GENERAL);

    CHECK(state.exception_pending);
    CHECK(state.next_pc == 0x80000180u);
    CHECK(state.cp0.byname.cp0r14_t.cp0r14_n.EPC == 0x80001000u);
    CHECK(CAUSE_BD(&state) == 0);
    CHECK(CAUSE_EXCCODE(&state) == EXC_SC);
    CHECK(STATUS_EXL(&state) == 1);
    return 0;
}

static int test_delay_slot_exception(void) {
    Registers state;

    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.pc = 0x80001004u;
    state.bds = 1;
    state.is_taken = 1;
    state.target_pc = 0x80002000u;

    raise_exception(&state, 0, EXC_BP, MIPS_VECTOR_GENERAL);

    CHECK(state.cp0.byname.cp0r14_t.cp0r14_n.EPC == 0x80001000u);
    CHECK(CAUSE_BD(&state) == 1);
    CHECK(state.next_pc == 0x80000180u);
    CHECK(!state.bds && !state.is_delay_slot && !state.is_taken);
    CHECK(state.target_pc == 0);
    return 0;
}

static int test_nested_exception_preserves_epc_bd(void) {
    Registers state;

    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status =
        SET_BITFIELD(0, CP0_STATUS_EXL_POS, CP0_STATUS_EXL_LEN, 1);
    state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(0, CP0_CAUSE_BD_POS, CP0_CAUSE_BD_LEN, 1);
    state.cp0.byname.cp0r14_t.cp0r14_n.EPC = 0x81234560u;
    state.pc = 0x80002000u;

    raise_exception(&state, 0, EXC_RI, MIPS_VECTOR_GENERAL);

    CHECK(state.cp0.byname.cp0r14_t.cp0r14_n.EPC == 0x81234560u);
    CHECK(CAUSE_BD(&state) == 1);
    CHECK(CAUSE_EXCCODE(&state) == EXC_RI);
    CHECK(state.next_pc == 0x80000180u);
    return 0;
}

static int test_tlb_vectors_and_state(void) {
    Registers state;
    const uint32_t va = 0x12345678u;

    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.cp0.byname.cp0r15_t.cp0r15_n.EBase = 0x81234000u;
    state.pc = 0x80003000u;

    raise_exception(&state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);

    CHECK(state.next_pc == 0x81234000u);
    CHECK(state.cp0.byname.cp0r8_t.cp0r8_n.BadVAddr == va);
    CHECK(GET_BITFIELD(state.cp0.byname.cp0r10_t.cp0r10_n.EntryHi,
                       13, 19) == (va >> 13));

    state.exception_pending = 0;
    state.next_pc = 0;
    raise_exception(&state, va, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
    CHECK(state.next_pc == 0x81234180u);
    return 0;
}

static int test_interrupt_and_cache_vectors(void) {
    Registers state;

    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(0, CP0_CAUSE_IV_POS, CP0_CAUSE_IV_LEN, 1);
    state.cp0.byname.cp0r15_t.cp0r15_n.EBase = 0x81234000u;
    state.pc = 0x80001000u;
    raise_exception(&state, 0, EXC_INT, MIPS_VECTOR_INTERRUPT);
    CHECK(state.next_pc == 0x81234200u);

    reset_cpu(&state);
    state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(0, CP0_CAUSE_IV_POS, CP0_CAUSE_IV_LEN, 1);
    state.pc = 0x80001000u;
    raise_exception(&state, 0, EXC_INT, MIPS_VECTOR_INTERRUPT);
    CHECK(state.next_pc == 0xbfc00400u);

    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.cp0.byname.cp0r15_t.cp0r15_n.EBase = 0x81234000u;
    state.pc = 0x80001000u;
    raise_exception(&state, 0, EXC_CAH, MIPS_VECTOR_CACHE_ERROR);
    CHECK(state.next_pc == 0xa1234100u);
    return 0;
}

static int test_soft_and_linux_reset(void) {
    Registers state;

    reset_cpu(&state);
    state.pc = 0x80004004u;
    state.bds = 1;
    state.gpr[16] = 0xfeedbeefu;
    state.tlb[3].entryhi = 0x12345042u;
    state.cp0.byname.cp0r18_t.cp0r18_n.WatchLo[0] = 1;

    soft_reset_cpu(&state);

    CHECK(state.pc == MIPS_RESET_VECTOR);
    CHECK(state.cp0.byname.cp0r30_t.cp0r30_n.ErrorEPC == 0x80004000u);
    CHECK(GET_BITFIELD(state.cp0.byname.cp0r12_t.cp0r12_n.Status,
                       CP0_STATUS_SR_POS, CP0_STATUS_SR_LEN) == 1);
    CHECK(STATUS_BEV(&state) == 1 && STATUS_ERL(&state) == 1);
    CHECK(state.gpr[16] == 0xfeedbeefu);
    CHECK(state.tlb[3].entryhi == 0x12345042u);
    CHECK(state.cp0.byname.cp0r18_t.cp0r18_n.WatchLo[0] == 0);

    linux_load_reset(&state);
    CHECK(state.pc == MIPS_LINUX_ENTRY);
    CHECK(state.next_pc == MIPS_LINUX_ENTRY + 4u);
    CHECK(state.cp0.byname.cp0r12_t.cp0r12_n.Status == 0);
    CHECK(state.gpr[4] == 0 && state.gpr[5] == 0);
    CHECK(state.gpr[6] == 0 && state.gpr[7] == 0);
    return 0;
}

int main(void) {
    int result;

    result = test_cold_reset();
    if (result) return result;
    result = test_reset_exception_scheduling();
    if (result) return result;
    result = test_precise_exception();
    if (result) return result;
    result = test_delay_slot_exception();
    if (result) return result;
    result = test_nested_exception_preserves_epc_bd();
    if (result) return result;
    result = test_tlb_vectors_and_state();
    if (result) return result;
    result = test_interrupt_and_cache_vectors();
    if (result) return result;
    return test_soft_and_linux_reset();
}
