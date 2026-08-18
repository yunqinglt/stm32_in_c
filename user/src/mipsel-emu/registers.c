#include "registers.h"
#include "exception.h"
#include <stdint.h>

// 2 = TLB Refill, 3 = TLB Invalid, 4 = TLB Modified
inline Result pfn_translate(uint32_t target, Registers *state, uint8_t is_write) {
    // kseg0: 0x8000_0000-0x9fff_ffff, kseg1: 0xa000_0000-0xbfff_ffff
    if (target >= 0x80000000 && target <= 0xBFFFFFFF) {
        // no need to translate
        return OK(target & 0x1FFFFFFF);
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
                    // TLB Invalid Exception
                    return ERR(3);
                }

                // !Dirty TLB Modified Exception
                if (is_write && !(elo & 0x04)) {
                    return ERR(4);
                }

                uint32_t page_offset_mask = even_odd_bit - 1;
                uint32_t offset = target & page_offset_mask;
                uint32_t pfn = (elo >> 6) & 0xFFFFFF;

                uint32_t pa = ((pfn << 12) & ~page_offset_mask) | offset;
                return OK(pa);
            }
        }
    }

    // TLB Refill Exception
    return ERR(2);
}