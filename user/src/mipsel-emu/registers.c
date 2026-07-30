#include "registers.h"


__static_inline uint32_t pfn_translate(uint32_t target, Registers *state) {
    if (target >= 0x80000000 && target <= 0xBFFFFFFF) {
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

                if (!(elo & 0x02)) {
                    return 0; 
                }

                uint32_t pfn = (elo >> 6) & 0xFFFFFF;
                uint32_t offset = target & (pmask | 0x1FFF);

                uint32_t pa = ((pfn << 12) & mask) | offset;
                return pa;
            }
        }
    }
    return 0;
}