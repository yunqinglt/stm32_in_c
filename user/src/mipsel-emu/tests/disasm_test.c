#include "disasm.h"

#include <stdio.h>
#include <string.h>

#define R(rs, rt, rd, sa, fn) \
    ((((uint32_t)(rs) & 31u) << 21) | (((uint32_t)(rt) & 31u) << 16) | \
     (((uint32_t)(rd) & 31u) << 11) | (((uint32_t)(sa) & 31u) << 6) | \
     ((uint32_t)(fn) & 63u))
#define I(op, rs, rt, imm) \
    ((((uint32_t)(op) & 63u) << 26) | (((uint32_t)(rs) & 31u) << 21) | \
     (((uint32_t)(rt) & 31u) << 16) | ((uint32_t)(imm) & 0xffffu))
#define J(op, target) \
    ((((uint32_t)(op) & 63u) << 26) | ((uint32_t)(target) & 0x03ffffffu))
#define COP0_MOVE(rs, rt, rd, sel) \
    I(0x10, (rs), (rt), (((uint32_t)(rd) & 31u) << 11) | ((sel) & 7u))
#define COP0_CO(fn) I(0x10, 0x10, 0, (fn))

typedef struct {
    uint32_t raw;
    const char *mnemonic;
} MnemonicCase;

static int check_mnemonics(void) {
    static const MnemonicCase cases[] = {
        /* Primary opcode leaf routes. */
        {J(0x02, 1), "j"}, {J(0x03, 1), "jal"},
        {I(0x04, 1, 2, 1), "beq"}, {I(0x05, 1, 2, 1), "bne"},
        {I(0x06, 1, 0, 1), "blez"}, {I(0x07, 1, 0, 1), "bgtz"},
        {I(0x08, 1, 2, 1), "addi"}, {I(0x09, 1, 2, 1), "addiu"},
        {I(0x0a, 1, 2, 1), "slti"}, {I(0x0b, 1, 2, 1), "sltiu"},
        {I(0x0c, 1, 2, 1), "andi"}, {I(0x0d, 1, 2, 1), "ori"},
        {I(0x0e, 1, 2, 1), "xori"}, {I(0x0f, 0, 2, 1), "lui"},
        {I(0x14, 1, 2, 1), "beql"}, {I(0x15, 1, 2, 1), "bnel"},
        {I(0x16, 1, 0, 1), "blezl"}, {I(0x17, 1, 0, 1), "bgtzl"},
        {J(0x1d, 1), "jalx"},
        {I(0x20, 1, 2, 1), "lb"}, {I(0x21, 1, 2, 1), "lh"},
        {I(0x22, 1, 2, 1), "lwl"}, {I(0x23, 1, 2, 1), "lw"},
        {I(0x24, 1, 2, 1), "lbu"}, {I(0x25, 1, 2, 1), "lhu"},
        {I(0x26, 1, 2, 1), "lwr"}, {I(0x28, 1, 2, 1), "sb"},
        {I(0x29, 1, 2, 1), "sh"}, {I(0x2a, 1, 2, 1), "swl"},
        {I(0x2b, 1, 2, 1), "sw"}, {I(0x2e, 1, 2, 1), "swr"},
        {I(0x2f, 1, 2, 1), "cache"}, {I(0x30, 1, 2, 1), "ll"},
        {I(0x33, 1, 2, 1), "pref"}, {I(0x38, 1, 2, 1), "sc"},

        /* SPECIAL leaf routes. */
        {R(0, 2, 3, 4, 0x00), "sll"},
        {R(0, 2, 3, 4, 0x02), "srl"},
        {R(0, 2, 3, 4, 0x03), "sra"},
        {R(1, 2, 3, 0, 0x04), "sllv"},
        {R(1, 2, 3, 0, 0x06), "srlv"},
        {R(1, 2, 3, 0, 0x07), "srav"},
        {R(31, 0, 0, 0, 0x08), "jr"},
        {R(31, 0, 16, 0, 0x09), "jalr"},
        {R(1, 2, 3, 0, 0x0a), "movz"},
        {R(1, 2, 3, 0, 0x0b), "movn"},
        {R(0, 0, 0, 0, 0x0c), "syscall"},
        {R(0, 0, 0, 0, 0x0d), "break"},
        {R(0, 0, 0, 0, 0x0f), "sync"},
        {R(0, 0, 3, 0, 0x10), "mfhi"},
        {R(3, 0, 0, 0, 0x11), "mthi"},
        {R(0, 0, 3, 0, 0x12), "mflo"},
        {R(3, 0, 0, 0, 0x13), "mtlo"},
        {R(1, 2, 0, 0, 0x18), "mult"},
        {R(1, 2, 0, 0, 0x19), "multu"},
        {R(1, 2, 0, 0, 0x1a), "div"},
        {R(1, 2, 0, 0, 0x1b), "divu"},
        {R(1, 2, 3, 0, 0x20), "add"},
        {R(1, 2, 3, 0, 0x21), "addu"},
        {R(1, 2, 3, 0, 0x22), "sub"},
        {R(1, 2, 3, 0, 0x23), "subu"},
        {R(1, 2, 3, 0, 0x24), "and"},
        {R(1, 2, 3, 0, 0x25), "or"},
        {R(1, 2, 3, 0, 0x26), "xor"},
        {R(1, 2, 3, 0, 0x27), "nor"},
        {R(1, 2, 3, 0, 0x2a), "slt"},
        {R(1, 2, 3, 0, 0x2b), "sltu"},
        {R(1, 2, 0, 0, 0x30), "tge"},
        {R(1, 2, 0, 0, 0x31), "tgeu"},
        {R(1, 2, 0, 0, 0x32), "tlt"},
        {R(1, 2, 0, 0, 0x33), "tltu"},
        {R(1, 2, 0, 0, 0x34), "teq"},
        {R(1, 2, 0, 0, 0x36), "tne"},
        {R(1, 2, 3, 0, 0x01), "movf"},
        {R(1, 3, 3, 0, 0x01), "movt"},
        {R(1, 2, 3, 4, 0x02), "rotr"},
        {R(1, 2, 3, 1, 0x06), "rotrv"},
        {R(31, 0, 0, 0x10, 0x08), "jr.hb"},
        {R(31, 0, 16, 0x10, 0x09), "jalr.hb"},

        /* REGIMM leaf routes. */
        {I(0x01, 1, 0x00, 1), "bltz"},
        {I(0x01, 1, 0x01, 1), "bgez"},
        {I(0x01, 1, 0x02, 1), "bltzl"},
        {I(0x01, 1, 0x03, 1), "bgezl"},
        {I(0x01, 1, 0x08, 1), "tgei"},
        {I(0x01, 1, 0x09, 1), "tgeiu"},
        {I(0x01, 1, 0x0a, 1), "tlti"},
        {I(0x01, 1, 0x0b, 1), "tltiu"},
        {I(0x01, 1, 0x0c, 1), "teqi"},
        {I(0x01, 1, 0x0e, 1), "tnei"},
        {I(0x01, 1, 0x10, 1), "bltzal"},
        {I(0x01, 1, 0x11, 1), "bgezal"},
        {I(0x01, 1, 0x12, 1), "bltzall"},
        {I(0x01, 1, 0x13, 1), "bgezall"},
        {I(0x01, 1, 0x1f, 1), "synci"},

        /* SPECIAL2 and SPECIAL3 leaf routes. */
        {I(0x1c, 1, 2, 0x00), "madd"},
        {I(0x1c, 1, 2, 0x01), "maddu"},
        {I(0x1c, 1, 2, (3u << 11) | 0x02), "mul"},
        {I(0x1c, 1, 2, 0x05), "msub"},
        {I(0x1c, 1, 2, 0x06), "msubu"},
        {I(0x1c, 1, 0, (3u << 11) | 0x20), "clz"},
        {I(0x1c, 1, 0, (3u << 11) | 0x21), "clo"},
        {I(0x1f, 1, 2, (7u << 11) | (8u << 6) | 0x00), "ext"},
        {I(0x1f, 1, 2, (15u << 11) | (8u << 6) | 0x04), "ins"},
        {I(0x1f, 0, 2, (3u << 11) | (2u << 6) | 0x20), "wsbh"},
        {I(0x1f, 0, 2, (3u << 11) | (16u << 6) | 0x20), "seb"},
        {I(0x1f, 0, 2, (3u << 11) | (24u << 6) | 0x20), "seh"},
        {I(0x1f, 0, 2, (29u << 11) | 0x3b), "rdhwr"},

        /* COP0 leaf routes. */
        {COP0_CO(0x01), "tlbr"}, {COP0_CO(0x02), "tlbwi"},
        {COP0_CO(0x06), "tlbwr"}, {COP0_CO(0x08), "tlbp"},
        {COP0_CO(0x18), "eret"}, {COP0_CO(0x1f), "deret"},
        {COP0_CO(0x20), "wait"},
        {COP0_MOVE(0x00, 2, 12, 0), "mfc0"},
        {COP0_MOVE(0x04, 2, 12, 0), "mtc0"},
        {COP0_MOVE(0x0a, 2, 3, 0), "rdpgpr"},
        {COP0_MOVE(0x0e, 2, 3, 0), "wrpgpr"},
        {COP0_MOVE(0x0b, 2, 12, 0), "di"},
        {COP0_MOVE(0x0b, 2, 12, 0) | 0x20u, "ei"},

        /* Direct coprocessor-unusable routes with known memory mnemonics. */
        {I(0x31, 1, 2, 1), "lwc1"}, {I(0x32, 1, 2, 1), "lwc2"},
        {I(0x35, 1, 2, 1), "ldc1"}, {I(0x36, 1, 2, 1), "ldc2"},
        {I(0x39, 1, 2, 1), "swc1"}, {I(0x3a, 1, 2, 1), "swc2"},
        {I(0x3d, 1, 2, 1), "sdc1"}, {I(0x3e, 1, 2, 1), "sdc2"},
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const char *actual = mips_mnemonic(cases[index].raw);
        if (strcmp(actual, cases[index].mnemonic) != 0) {
            fprintf(stderr, "mnemonic[%zu]: raw=%08x expected=%s actual=%s\n",
                    index, cases[index].raw, cases[index].mnemonic, actual);
            return 1;
        }
    }
    return 0;
}

static int expect_disassembly(uint32_t pc, uint32_t raw,
                              const char *expected) {
    char buffer[192];

    mips_disassemble(pc, raw, buffer, sizeof(buffer));
    if (strcmp(buffer, expected) != 0) {
        fprintf(stderr, "disassembly: raw=%08x\nexpected: %s\nactual:   %s\n",
                raw, expected, buffer);
        return 1;
    }
    return 0;
}

static int check_formatting(void) {
    char tiny[5] = {'x', 'x', 'x', 'x', 'x'};

    if (expect_disassembly(0x1000u, I(0x04, 4, 5, 0xfffe),
                           "beq $a0, $a1, 0x00000ffc")) return 1;
    if (expect_disassembly(0x0ffffffcu, J(0x02, 1),
                           "j 0x10000004")) return 1;
    if (expect_disassembly(0, I(0x09, 29, 29, 0xfffc),
                           "addiu $sp, $sp, -4")) return 1;
    if (expect_disassembly(0, I(0x23, 29, 2, 16),
                           "lw $v0, 16($sp)")) return 1;
    if (expect_disassembly(0, I(0x01, 4, 0x1f, 12),
                           "synci 12($a0)")) return 1;
    if (expect_disassembly(0,
                           I(0x1f, 3, 2,
                             (7u << 11) | (8u << 6) | 0x00),
                           "ext $v0, $v1, 8, 8")) return 1;
    if (expect_disassembly(0,
                           I(0x1f, 3, 2,
                             (15u << 11) | (8u << 6) | 0x04),
                           "ins $v0, $v1, 8, 8")) return 1;
    if (expect_disassembly(0, COP0_MOVE(0x00, 2, 12, 3),
                           "mfc0 $v0, $12, 3")) return 1;
    if (expect_disassembly(0, I(0x18, 0, 0, 0),
                           ".word 0x60000000 # reserved instruction")) return 1;
    if (expect_disassembly(0, I(0x31, 4, 2, 8),
                           "lwc1 $f2, 8($a0) # coprocessor 1 unusable")) return 1;
    if (expect_disassembly(0, I(0x11, 0, 0, 0),
                           ".word 0x44000000 # coprocessor 1 unusable")) return 1;

    mips_disassemble(0, I(0x09, 0, 2, 1), tiny, sizeof(tiny));
    if (tiny[sizeof(tiny) - 1] != '\0') {
        fprintf(stderr, "small output buffer was not terminated\n");
        return 1;
    }
    mips_disassemble(0, 0, NULL, 0);
    return 0;
}

static int check_classification(void) {
    uint32_t invalid_ext = I(0x1f, 1, 2,
                             (31u << 11) | (1u << 6) | 0x00);
    uint32_t invalid_ins = I(0x1f, 1, 2,
                             (3u << 11) | (4u << 6) | 0x04);

    if (mips_decode_kind(I(0x09, 1, 2, 3)) != MIPS_DECODE_VALID)
        return 1;
    if (mips_decode_kind(I(0x18, 0, 0, 0)) != MIPS_DECODE_RESERVED)
        return 1;
    if (mips_decode_kind(I(0x11, 0, 0, 0)) !=
        MIPS_DECODE_COPROCESSOR_UNUSABLE)
        return 1;
    if (mips_decode_kind(invalid_ext) != MIPS_DECODE_RESERVED)
        return 1;
    if (mips_decode_kind(invalid_ins) != MIPS_DECODE_RESERVED)
        return 1;
    return 0;
}

int main(void) {
    if (check_mnemonics()) return 1;
    if (check_formatting()) return 2;
    if (check_classification()) return 3;
    return 0;
}
