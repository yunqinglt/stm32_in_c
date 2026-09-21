#include "emu.h"
#include "observer.h"
#include "op.h"
#include "registers.h"
#include "exception.h"
#include "platform.h"
#include <stdint.h>

/*
 * The switch dispatcher and all its instruction bodies intentionally share
 * one translation unit.  Every body is static/always_inline in op.c, so there
 * is no call/return boundary between decode and execution.
 */
#include "op.c"
// CPU: MIPS32 Little Endian Release 2 no FPU / Version 0.1
/*
    [*] = archived
    [ ] = todo
    [/] = doing
    [-] = not planned
    [x] = failed

    Features: 
    [*] single pipeline
    [*] basic mips32 release 2 behavior
    [x] Cache and Sync (C-based emulator has no hazard)
    [*] branch delay
    ------ Boot Linux kernel ------
    [*] virtual mmio device
    [*] ->   16550A UART Console
    [ ] ->   Display
    [/] Multi-Thread Application-Specific Extension
    [ ] ->   shadow register set
    [ ] SmartMIPS Application-Specific Extension
    [*] virtual debug support
    [*] ->   ncurses register/instruction/exception TUI
    [ ] ->   GDB

    [-] FPU
    [-] XPA eXtended Physical Address
    [-] 5-way pipeline simulation
    
    [*] VPS Variable Page Sizes
*/

/*
 * The values are architectural opcode values, not an ABI.  Keeping the
 * decoding vocabulary here lets the compiler emit direct calls from the
 * switch rather than retaining a 400-entry function-pointer lookup table.
 */
typedef enum {
    MIPSEL_OPCODE_SPECIAL  = 0x00,
    MIPSEL_OPCODE_REGIMM   = 0x01,
    MIPSEL_OPCODE_J        = 0x02,
    MIPSEL_OPCODE_JAL      = 0x03,
    MIPSEL_OPCODE_BEQ      = 0x04,
    MIPSEL_OPCODE_BNE      = 0x05,
    MIPSEL_OPCODE_BLEZ     = 0x06,
    MIPSEL_OPCODE_BGTZ     = 0x07,
    MIPSEL_OPCODE_ADDI     = 0x08,
    MIPSEL_OPCODE_ADDIU    = 0x09,
    MIPSEL_OPCODE_SLTI     = 0x0a,
    MIPSEL_OPCODE_SLTIU    = 0x0b,
    MIPSEL_OPCODE_ANDI     = 0x0c,
    MIPSEL_OPCODE_ORI      = 0x0d,
    MIPSEL_OPCODE_XORI     = 0x0e,
    MIPSEL_OPCODE_LUI      = 0x0f,
    MIPSEL_OPCODE_COP0     = 0x10,
    MIPSEL_OPCODE_COP1     = 0x11,
    MIPSEL_OPCODE_COP2     = 0x12,
    MIPSEL_OPCODE_COP3     = 0x13,
    MIPSEL_OPCODE_BEQL     = 0x14,
    MIPSEL_OPCODE_BNEL     = 0x15,
    MIPSEL_OPCODE_BLEZL    = 0x16,
    MIPSEL_OPCODE_BGTZL    = 0x17,
    MIPSEL_OPCODE_SPECIAL2 = 0x1c,
    MIPSEL_OPCODE_JALX     = 0x1d,
    MIPSEL_OPCODE_SPECIAL3 = 0x1f,
    MIPSEL_OPCODE_LB       = 0x20,
    MIPSEL_OPCODE_LH       = 0x21,
    MIPSEL_OPCODE_LWL      = 0x22,
    MIPSEL_OPCODE_LW       = 0x23,
    MIPSEL_OPCODE_LBU      = 0x24,
    MIPSEL_OPCODE_LHU      = 0x25,
    MIPSEL_OPCODE_LWR      = 0x26,
    MIPSEL_OPCODE_SB       = 0x28,
    MIPSEL_OPCODE_SH       = 0x29,
    MIPSEL_OPCODE_SWL      = 0x2a,
    MIPSEL_OPCODE_SW       = 0x2b,
    MIPSEL_OPCODE_SWR      = 0x2e,
    MIPSEL_OPCODE_CACHE    = 0x2f,
    MIPSEL_OPCODE_LL       = 0x30,
    MIPSEL_OPCODE_PREF     = 0x33,
    MIPSEL_OPCODE_SC       = 0x38,
} mipsel_opcode_t;

void execute_instr(uint32_t instr, Registers *state) {
    switch (getop(instr)) {
        case MIPSEL_OPCODE_SPECIAL:  special1_handler(instr, state); return;
        case MIPSEL_OPCODE_REGIMM:   regimm_handler(instr, state); return;
        case MIPSEL_OPCODE_J:        op_j(instr, state); return;
        case MIPSEL_OPCODE_JAL:      op_jal(instr, state); return;
        case MIPSEL_OPCODE_BEQ:      op_beq(instr, state); return;
        case MIPSEL_OPCODE_BNE:      op_bne(instr, state); return;
        case MIPSEL_OPCODE_BLEZ:     op_blez(instr, state); return;
        case MIPSEL_OPCODE_BGTZ:     op_bgtz(instr, state); return;
        case MIPSEL_OPCODE_ADDI:     op_addi(instr, state); return;
        case MIPSEL_OPCODE_ADDIU:    op_addiu(instr, state); return;
        case MIPSEL_OPCODE_SLTI:     op_slti(instr, state); return;
        case MIPSEL_OPCODE_SLTIU:    op_sltiu(instr, state); return;
        case MIPSEL_OPCODE_ANDI:     op_andi(instr, state); return;
        case MIPSEL_OPCODE_ORI:      op_ori(instr, state); return;
        case MIPSEL_OPCODE_XORI:     op_xori(instr, state); return;
        case MIPSEL_OPCODE_LUI:      op_lui(instr, state); return;
        case MIPSEL_OPCODE_COP0:     op_cop0_handler(instr, state); return;
        case MIPSEL_OPCODE_COP1:
        case MIPSEL_OPCODE_COP2:
        case MIPSEL_OPCODE_COP3:     delta(instr, state); return;
        case MIPSEL_OPCODE_BEQL:     op_beql(instr, state); return;
        case MIPSEL_OPCODE_BNEL:     op_bnel(instr, state); return;
        case MIPSEL_OPCODE_BLEZL:    op_blezl(instr, state); return;
        case MIPSEL_OPCODE_BGTZL:    op_bgtzl(instr, state); return;
        case MIPSEL_OPCODE_SPECIAL2: special2_handler(instr, state); return;
        case MIPSEL_OPCODE_JALX:     op_jalx(instr, state); return;
        case MIPSEL_OPCODE_SPECIAL3: special3_handler(instr, state); return;
        case MIPSEL_OPCODE_LB:       op_lb(instr, state); return;
        case MIPSEL_OPCODE_LH:       op_lh(instr, state); return;
        case MIPSEL_OPCODE_LWL:      op_lwl(instr, state); return;
        case MIPSEL_OPCODE_LW:       op_lw(instr, state); return;
        case MIPSEL_OPCODE_LBU:      op_lbu(instr, state); return;
        case MIPSEL_OPCODE_LHU:      op_lhu(instr, state); return;
        case MIPSEL_OPCODE_LWR:      op_lwr(instr, state); return;
        case MIPSEL_OPCODE_SB:       op_sb(instr, state); return;
        case MIPSEL_OPCODE_SH:       op_sh(instr, state); return;
        case MIPSEL_OPCODE_SWL:      op_swl(instr, state); return;
        case MIPSEL_OPCODE_SW:       op_sw(instr, state); return;
        case MIPSEL_OPCODE_SWR:      op_swr(instr, state); return;
        case MIPSEL_OPCODE_CACHE:    op_cache(instr, state); return;
        case MIPSEL_OPCODE_LL:       op_ll(instr, state); return;
        case MIPSEL_OPCODE_PREF:     op_pref(instr, state); return;
        case MIPSEL_OPCODE_SC:       op_sc(instr, state); return;

        /* MIPS64, coprocessor memory and unimplemented encodings. */
        case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1e:
        case 0x27: case 0x2c: case 0x2d: case 0x34: case 0x37:
        case 0x3b: case 0x3c: case 0x3f:
            beta(instr, state);
            return;

        /* Coprocessor loads/stores must raise Coprocessor Unusable. */
        case 0x31: case 0x32: case 0x35: case 0x36:
        case 0x39: case 0x3a: case 0x3d: case 0x3e:
            delta(instr, state);
            return;

        default:
            beta(instr, state);
            return;
    }
}

static void commit_pending_exception(Registers *state) {
    state->pc = state->next_pc;
    state->next_pc = state->pc + 4u;
    state->exception_pending = 0;
}

void cpu_step(Registers *state) {
    uint32_t instr;

    /* Also handles an interrupt or other exception raised between steps. */
    if (state->exception_pending) {
        commit_pending_exception(state);
        return;
    }

    /* Interrupts are sampled between instructions. */
    platform_update_interrupts(state);
    if (STATUS_IE(state) && !STATUS_EXL(state) && !STATUS_ERL(state) &&
        (STATUS_IM(state) & CAUSE_IP(state))) {
        raise_exception(state, 0, EXC_INT, MIPS_VECTOR_INTERRUPT);
        commit_pending_exception(state);
        return;
    }

    if (state->pc & 0x03) {
        raise_exception(state, state->pc, EXC_AdEL, MIPS_VECTOR_GENERAL);
        commit_pending_exception(state);
        return;
    }

    Result pa = pfn_translate(state->pc, state, 0);
    if (!TEST_RESULT(pa)) {
        switch (pa.value.reason) {
            case 2:
                raise_exception(state, state->pc, EXC_TLBL, MIPS_VECTOR_TLB_REFILL);
                commit_pending_exception(state);
                return;
            case 3:
                raise_exception(state, state->pc, EXC_TLBL, MIPS_VECTOR_GENERAL);
                commit_pending_exception(state);
                return;
        }
    }

    instr = read32((uint32_t) pa.value.ok);
    mipsel_emu_observer_instruction_begin(state->pc,
                                          (uint32_t)pa.value.ok, instr,
                                          state);
    execute_instr(instr, state);
    state->gpr[0] = 0;

    /* A synchronous instruction exception wins over normal/branch commit. */
    if (state->exception_pending) {
        commit_pending_exception(state);
        mipsel_emu_observer_instruction_end(state);
        return;
    }

    state->bds = 0;

    if (state->is_delay_slot) {
        state->bds = 1;

        if (state->is_taken) {
            state->pc = state->next_pc; // branch taken
            state->next_pc = state->target_pc;
            state->is_delay_slot = 0;
            state->is_taken = 0;
            mipsel_emu_observer_instruction_end(state);
            return;
        } else {
            state->pc += 4; // branch not taken
        }

        state->is_delay_slot = 0;
        state->is_taken = 0;
    } else {
        state->pc = state->next_pc; // not branch
    }

    state->next_pc += 4;
    mipsel_emu_observer_instruction_end(state);
}

void update_cycle(Registers *state) {
    decrease_random(state);
    if (!increase_counter(state))
        return;

    if (state->cp0.byname.cp0r9_t.cp0r9_n.Count ==
        state->cp0.byname.cp0r11_t.cp0r11_n.Compare) {
        uint32_t cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        cause = SET_BITFIELD(cause, CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN, 1);
        cause = SET_BITFIELD(cause, CP0_CAUSE_IP_POS + 7, 1, 1);
        state->cp0.byname.cp0r13_t.cp0r13_n.Cause = cause;
    }
}

void mipsel_emu_step(Registers *state) {
    if (!state) return;
    cpu_step(state);
    update_cycle(state);
}

uint32_t mipsel_emu_run_steps(Registers *state, uint32_t budget) {
    uint32_t completed = 0;

    if (!state) return 0;
    while (completed < budget) {
        mipsel_emu_step(state);
        ++completed;
    }
    return completed;
}
