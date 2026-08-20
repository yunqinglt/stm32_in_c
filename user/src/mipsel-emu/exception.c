#include "exception.h"
#include "observer.h"
#include "registers.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define GENERAL_VECTOR_OFFSET       0x180u
#define TLB_REFILL_VECTOR_OFFSET    0x000u
#define INTERRUPT_VECTOR_OFFSET     0x200u
#define CACHE_ERROR_VECTOR_OFFSET   0x100u

static uint32_t exception_resume_pc(const Registers *state) {
    return state->bds ? state->pc - 4u : state->pc;
}

static void flush_control_transfer(Registers *state) {
    state->is_delay_slot = 0;
    state->is_taken = 0;
    state->target_pc = 0;
    state->bds = 0;
}

static uint32_t ebase_address(const Registers *state) {
    uint32_t ebase =
        state->cp0.byname.cp0r15_t.cp0r15_n.EBase & 0xfffff000u;

    return ebase ? ebase : MIPS_DEFAULT_EBASE;
}

static uint32_t general_vector(const Registers *state) {
    if (STATUS_BEV(state)) {
        return MIPS_RESET_VECTOR + 0x380u;
    }

    return ebase_address(state) + GENERAL_VECTOR_OFFSET;
}

static uint32_t interrupt_vector(const Registers *state) {
    const uint32_t cause =
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
    const bool use_special_vector =
        GET_BITFIELD(cause, CP0_CAUSE_IV_POS, CP0_CAUSE_IV_LEN) != 0;

    if (!use_special_vector) {
        return general_vector(state);
    }

    if (STATUS_BEV(state)) {
        return MIPS_RESET_VECTOR + 0x400u;
    }

    return ebase_address(state) + INTERRUPT_VECTOR_OFFSET;
}

static uint32_t tlb_refill_vector(const Registers *state,
                                  bool already_in_exception) {
    if (already_in_exception) {
        return general_vector(state);
    }

    if (STATUS_BEV(state)) {
        return MIPS_RESET_VECTOR + 0x200u;
    }

    return ebase_address(state) + TLB_REFILL_VECTOR_OFFSET;
}

static uint32_t cache_error_vector(const Registers *state) {
    if (STATUS_BEV(state)) {
        return MIPS_RESET_VECTOR + 0x300u;
    }

    /* Cache errors execute through the uncached alias of EBase. */
    return 0xa0000000u |
           (ebase_address(state) & 0x1ffff000u) |
           CACHE_ERROR_VECTOR_OFFSET;
}

static void update_tlb_exception_state(Registers *state, uint32_t va) {
    state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr = va;
    state->cp0.byname.cp0r4_t.cp0r4_n.Context =
        SET_BITFIELD(state->cp0.byname.cp0r4_t.cp0r4_n.Context,
                     CP0_CONTEXT_BVPN2_POS, CP0_CONTEXT_BVPN2_LEN,
                     va >> 13);
    state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi =
        SET_BITFIELD(state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi,
                     13, 19, va >> 13);
}

void reset_cpu(Registers *state) {
    memset(state, 0, sizeof(*state));

    state->pc = MIPS_RESET_VECTOR;
    state->next_pc = MIPS_RESET_VECTOR + 4u;

    state->cp0.byname.cp0r1_t.cp0r1_n.Random = INIT_RANDOM;
    state->cp0.byname.cp0r5_t.cp0r5_n.PageMask = 0;
    state->cp0.byname.cp0r6_t.cp0r6_n.Wired = 0;

    state->cp0.byname.cp0r12_t.cp0r12_n.Status = INIT_STATUS;
    state->cp0.byname.cp0r12_t.cp0r12_n.IntCtl = INIT_INTCTL;
    state->cp0.byname.cp0r12_t.cp0r12_n.SRSCtl = 0;

    state->cp0.byname.cp0r15_t.cp0r15_n.PRId = INIT_PRID;
    state->cp0.byname.cp0r15_t.cp0r15_n.EBase = MIPS_DEFAULT_EBASE;

    state->cp0.byname.cp0r16_t.cp0r16_n.Config = INIT_CONFIG0_R2;
    state->cp0.byname.cp0r16_t.cp0r16_n.Config1 = INIT_CONFIG1;
}

void soft_reset_cpu(Registers *state) {
    uint32_t status = state->cp0.byname.cp0r12_t.cp0r12_n.Status;

    state->cp0.byname.cp0r30_t.cp0r30_n.ErrorEPC =
        exception_resume_pc(state);

    status = SET_BITFIELD(status, CP0_STATUS_SR_POS,
                          CP0_STATUS_SR_LEN, 1);
    status = SET_BITFIELD(status, CP0_STATUS_BEV_POS,
                          CP0_STATUS_BEV_LEN, 1);
    status = SET_BITFIELD(status, CP0_STATUS_ERL_POS,
                          CP0_STATUS_ERL_LEN, 1);
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = status;

    if (!STATUS_EXL(state)) {
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
            SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,
                         CP0_CAUSE_BD_POS, CP0_CAUSE_BD_LEN, 0);
    }

    for (uint32_t i = 0; i < 8; ++i) {
        state->cp0.byname.cp0r18_t.cp0r18_n.WatchLo[i] = 0;
    }

    state->ll_bit = 0;
    state->ll_addr = 0;
    state->ISAMode = 0;
    state->exception_pending = 0;
    flush_control_transfer(state);

    state->pc = MIPS_RESET_VECTOR;
    state->next_pc = MIPS_RESET_VECTOR + 4u;
}

void linux_load_reset(Registers *state) {
    reset_cpu(state);

    /* Direct kernel entry models a bootloader hand-off, not reset mode.
     * The board loader replaces this legacy fallback PC with ELF e_entry
     * and fills the UHI/FDT argument registers.
     */
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state->pc = MIPS_LINUX_ENTRY;
    state->next_pc = MIPS_LINUX_ENTRY + 4u;
}

void raise_exception(Registers *state, uint32_t exc_info,
                     uint8_t exc_code, VectorClass class) {
    const bool already_in_exception = STATUS_EXL(state) != 0;
    const bool in_delay_slot = state->bds != 0;
    uint32_t vector;

    /* Reset-class events do not write ExcCode/EPC or enter EXL. */
    if (class == MIPS_VECTOR_RESET) {
        if (exc_code == EXC_RESET) {
            reset_cpu(state);
            state->next_pc = state->pc;
            state->exception_pending = 1;
        } else if (exc_code == EXC_SRES) {
            soft_reset_cpu(state);
            state->next_pc = state->pc;
            state->exception_pending = 1;
        }
        mipsel_emu_observer_exception(state, exc_info, exc_code, class);
        return;
    }

    state->ll_bit = 0;
    state->ll_addr = 0;

    state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
        SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,
                     CP0_CAUSE_EXCCODE_POS, CP0_CAUSE_EXCCODE_LEN,
                     exc_code);

    /* EPC and BD belong to the first exception in an EXL nesting chain. */
    if (!already_in_exception) {
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
            SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,
                         CP0_CAUSE_BD_POS, CP0_CAUSE_BD_LEN,
                         in_delay_slot);
        state->cp0.byname.cp0r14_t.cp0r14_n.EPC =
            in_delay_slot ? state->pc - 4u : state->pc;
    }

    if (exc_code == EXC_CpU) {
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
            SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,
                         CP0_CAUSE_CE_POS, CP0_CAUSE_CE_LEN,
                         exc_info);
    }

    if (exc_code == EXC_AdEL || exc_code == EXC_AdES) {
        state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr = exc_info;
    }

    if (exc_code == EXC_TLBL || exc_code == EXC_TLBS ||
        exc_code == EXC_MOD) {
        update_tlb_exception_state(state, exc_info);
    }

    switch (class) {
        case MIPS_VECTOR_INTERRUPT:
            vector = interrupt_vector(state);
            break;
        case MIPS_VECTOR_TLB_REFILL:
            vector = tlb_refill_vector(state, already_in_exception);
            break;
        case MIPS_VECTOR_CACHE_ERROR:
            vector = cache_error_vector(state);
            break;
        case MIPS_VECTOR_GENERAL:
        default:
            vector = general_vector(state);
            break;
    }

    state->cp0.byname.cp0r12_t.cp0r12_n.Status =
        SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status,
                     CP0_STATUS_EXL_POS, CP0_STATUS_EXL_LEN, 1);

    state->ISAMode = 0;
    flush_control_transfer(state);
    state->next_pc = vector;
    state->exception_pending = 1;
    mipsel_emu_observer_exception(state, exc_info, exc_code, class);
}
