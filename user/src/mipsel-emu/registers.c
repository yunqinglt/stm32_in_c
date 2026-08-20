#include "registers.h"
#include "exception.h"
#include <stdint.h>

// 2 = TLB Refill, 3 = TLB Invalid, 4 = TLB Modified
Result pfn_translate(uint32_t target, Registers *state, uint8_t is_write) {
    // kseg0: 0x8000_0000-0x9fff_ffff, kseg1: 0xa000_0000-0xbfff_ffff
    if (target >= 0x80000000 && target <= 0xBFFFFFFF) {
        // no need to translate
        return OK(target & 0x1FFFFFFF);
    }

    /* Status.ERL makes kuseg an unmapped, uncached physical window. */
    if (STATUS_ERL(state) && target < 0x80000000u) {
        return OK(target);
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

bool mipsel_cp0_write(Registers *state, unsigned reg, unsigned sel,
                      uint32_t value) {
    if (!state || reg >= 32u || sel >= 8u) return false;

    if (reg == 13u && sel == 0u) {
        /* Cause only exposes software interrupt and control bits as RW. */
        const uint32_t writable =
            (UINT32_C(1) << CP0_CAUSE_DC_POS) |
            (UINT32_C(1) << CP0_CAUSE_PCI_POS) |
            (UINT32_C(1) << CP0_CAUSE_IV_POS) |
            (UINT32_C(1) << CP0_CAUSE_WP_POS) |
            (UINT32_C(3) << CP0_CAUSE_IP_POS);
        uint32_t old = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause =
            (old & ~writable) | (value & writable);
    } else {
        state->cp0.regs[reg][sel] = value;
    }

    /* Writing Compare acknowledges the MIPS Count/Compare interrupt. */
    if (reg == 11u && sel == 0u) {
        uint32_t cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        cause = SET_BITFIELD(cause, CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN, 0);
        cause = SET_BITFIELD(cause, CP0_CAUSE_IP_POS + 7, 1, 0);
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause = cause;
    }
    return true;
}
