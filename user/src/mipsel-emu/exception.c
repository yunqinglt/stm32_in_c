#include "exception.h"
#include "registers.h"
#include <stdint.h>

// for startup reset initial
void reset_cpu(Registers *state) {
    state->cp0.byname.cp0r1_t.cp0r1_n.Random = 63;
    state->cp0.byname.cp0r5_t.cp0r5_n.PageMask = 0;
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = INIT_STATUS;

    // to be implemented
    state->cp0.byname.cp0r12_t.cp0r12_n.SRSCtl = 0;

    state->cp0.byname.cp0r16_t.cp0r16_n.Config = INIT_CONFIG0_R1;

}

// Building, new exception handler
void raise_exception(Registers *state, uint32_t exc_info, uint8_t exc_code, VectorClass class) {
    state->ll_bit = 0;
    state->cp0.byname.cp0r13_t.cp0r13_n.Cause = 
        SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,\
            CP0_CAUSE_EXCCODE_POS, CP0_CAUSE_EXCCODE_LEN, exc_code);

    // branch delay slot?
    state->cp0.byname.cp0r14_t.cp0r14_n.EPC = state->pc;

    switch (class) {
        // will not implement interrupt vector
        case MIPS_VECTOR_INTERRUPT:
        case MIPS_VECTOR_GENERAL:
            // more specific exception will be ported here.
            if (exc_code == EXC_CpU) {
                state->cp0.byname.cp0r13_t.cp0r13_n.Cause = 
                    SET_BITFIELD(state->cp0.byname.cp0r13_t.cp0r13_n.Cause,\
                        CP0_CAUSE_CE_POS, CP0_CAUSE_CE_LEN, exc_info);
            }

            if ((exc_code == EXC_AdEL) || (exc_code == EXC_AdES)) {
                state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr = exc_info; // BadVAddr
            }

            if ((exc_code == EXC_TLBL) || (exc_code == EXC_TLBS) || (exc_code == EXC_MOD)) {
                state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr = exc_info; // BadVAddr
                state->cp0.byname.cp0r4_t.cp0r4_n.Context =
                    SET_BITFIELD(state->cp0.byname.cp0r4_t.cp0r4_n.Context,\
                        CP0_CONTEXT_BVPN2_POS, CP0_CONTEXT_BVPN2_LEN, (exc_info >> 13)); // BadVPN2

                state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi = 
                    SET_BITFIELD(state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi,\
                        13, 19, (exc_info >> 13)); // EntryHi
            }

            // Interrupt?

            state->next_pc = STATUS_BEV(state) ? 0xbfc00380 : 0x80000180;

            break;
            
        case MIPS_VECTOR_TLB_REFILL:
            // exc_info = VA
            state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr = exc_info; // BadVAddr
            state->cp0.byname.cp0r4_t.cp0r4_n.Context =
                SET_BITFIELD(state->cp0.byname.cp0r4_t.cp0r4_n.Context,\
                    CP0_CONTEXT_BVPN2_POS, CP0_CONTEXT_BVPN2_LEN, (exc_info >> 13)); // BadVPN2

            state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi = 
                SET_BITFIELD(state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi,\
                    13, 19, (exc_info >> 13)); // EntryHi

            state->next_pc = refill_vector[STATUS_BEV(state)][STATUS_EXL(state)];
            break;

        case MIPS_VECTOR_RESET:
            if (exc_code == EXC_RESET) reset_cpu(state);
            if (exc_code == EXC_SRES); // TODO
            break;

        case MIPS_VECTOR_CACHE_ERROR:
            state->next_pc = STATUS_BEV(state) ? 0xbfc00300 : 0xa0000100;
            break;
    }

    // set Status.EXL
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = 
        SET_BITFIELD(state->cp0.byname.cp0r12_t.cp0r12_n.Status,\
            CP0_STATUS_EXL_POS, CP0_STATUS_EXL_LEN, 1);
}