#include "registers.h"
#include "exception.h"

// op_xx should not do anything when addr == 0
__STATIC_FORCEINLINE uint32_t pfn_translate(uint32_t target, Registers *state, uint8_t is_write) {
    if (target & 0x03) {
        // Address unaligned
        trigger_exception_helper(EXC_AdEL, state, target);
        return 0;
    }

    if (target >= 0x80000000 && target <= 0xBFFFFFFF) {
        // no need to translate
        return target & 0x1FFFFFFF;
    }

    uint8_t current_asid = state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi & 0xFF;

    for (int i = 0; i < 64; ++i) {
        uint32_t pmask = state->tlb[i].pmask;
        uint32_t ehi   = state->tlb[i].entryhi;
        uint32_t elo0  = state->tlb[i].entrylo0;
        uint32_t elo1  = state->tlb[i].entrylo1;

        uint32_t mask = ~(pmask | 0x1FFF);

        if ((target & mask) == (ehi & mask)) {
            uint8_t is_global = (elo0 & 1) & (elo1 & 1);
            
            if (is_global || (ehi & 0xFF) == current_asid) {
                uint32_t even_odd_bit = ((pmask | 0x1FFF) + 1) >> 1;
                uint32_t elo = (target & even_odd_bit) ? elo1 : elo0;

                // !Valid
                if (!(elo & 0x02)) {
                    // op_xx should rewrite state->Cause by its type
                    // TLB Invalid Exception
                    trigger_exception_helper(EXC_TLBL, state, target);
                    return 0; 
                }

                // !Dirty
                if (is_write && !(elo & 0x04)) {
                    trigger_exception_helper(EXC_MOD, state, target);
                    return 0;
                }

                uint32_t pfn = (elo >> 6) & 0xFFFFFF;
                uint32_t offset = target & (pmask | 0x1FFF);

                uint32_t pa = ((pfn << 12) & mask) | offset;
                return pa;
            }
        }
    }

    // TLB Refill Exception
    trigger_exception_helper(EXC_TLBL, state, target);
    return 0;
}

__STATIC_FORCEINLINE void trigger_exception_helper(uint32_t exc, Registers *state, uint32_t exc_info) {
    // next_pc will be rewrite at this section

    switch (exc) {
        case EXC_RESET:
            reset_cpu(state);
            break;

        case EXC_CpU:
            state->cp0.byname.cp0r13_t.cp0r13_n.Cause = 
                SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,\
                     CP0_CAUSE_CE_POS, CP0_CAUSE_CE_LEN, exc_info);
            break;

        case EXC_AdEL:
        case EXC_AdES:
        case EXC_TLBL:
        case EXC_TLBS:
            state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr = exc_info; // BadVAddr
            state->cp0.byname.cp0r4_t.cp0r4_n.Context =
                SET_BITFIELD(state->cp0.byname.cp0r4_t.cp0r4_n.Context,\
                    CP0_CONTEXT_BVPN2_POS, CP0_CONTEXT_BVPN2_LEN, (exc_info >> 13)); // BadVPN2

            state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi = 
                SET_BITFIELD(state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi,\
                    13, 19, (exc_info >> 13)); // EntryHi
            break;


    }
}