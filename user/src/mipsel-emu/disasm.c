#include "disasm.h"

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    FMT_RAW,
    FMT_NONE,
    FMT_JUMP,
    FMT_BRANCH_RS_RT,
    FMT_BRANCH_RS,
    FMT_RT_RS_SIGNED,
    FMT_RT_RS_UNSIGNED,
    FMT_RT_UNSIGNED,
    FMT_MEM_GPR,
    FMT_MEM_BASE,
    FMT_MEM_FPR,
    FMT_MEM_COP,
    FMT_CACHE,
    FMT_RD_RT_SA,
    FMT_RD_RT_RS,
    FMT_RS,
    FMT_RD_RS,
    FMT_RD_RS_RT,
    FMT_RS_RT,
    FMT_TRAP_RS_RT,
    FMT_CODE20,
    FMT_SYNC,
    FMT_RD,
    FMT_TRAP_RS_IMM,
    FMT_EXT,
    FMT_INS,
    FMT_RD_RT,
    FMT_RDHWR,
    FMT_CP0_MOVE,
    FMT_PGPR,
    FMT_DI_EI,
} OperandFormat;

typedef struct {
    const char *mnemonic;
    OperandFormat format;
    MipsDecodeKind kind;
    unsigned coprocessor;
} DecodedInstruction;

static const char *const gpr_names[32] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0",   "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0",   "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8",   "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
};

static unsigned field_rs(uint32_t raw) { return (raw >> 21) & 0x1fu; }
static unsigned field_rt(uint32_t raw) { return (raw >> 16) & 0x1fu; }
static unsigned field_rd(uint32_t raw) { return (raw >> 11) & 0x1fu; }
static unsigned field_sa(uint32_t raw) { return (raw >> 6) & 0x1fu; }
static unsigned field_fn(uint32_t raw) { return raw & 0x3fu; }

static DecodedInstruction valid(const char *mnemonic, OperandFormat format) {
    DecodedInstruction decoded = {
        .mnemonic = mnemonic,
        .format = format,
        .kind = MIPS_DECODE_VALID,
        .coprocessor = 0,
    };
    return decoded;
}

static DecodedInstruction reserved(void) {
    DecodedInstruction decoded = {
        .mnemonic = "reserved",
        .format = FMT_RAW,
        .kind = MIPS_DECODE_RESERVED,
        .coprocessor = 0,
    };
    return decoded;
}

static DecodedInstruction coprocessor_unusable(const char *mnemonic,
                                                OperandFormat format,
                                                unsigned coprocessor) {
    DecodedInstruction decoded = {
        .mnemonic = mnemonic,
        .format = format,
        .kind = MIPS_DECODE_COPROCESSOR_UNUSABLE,
        .coprocessor = coprocessor,
    };
    return decoded;
}

static DecodedInstruction decode_special(uint32_t raw) {
    switch (field_fn(raw)) {
        case 0x00: return valid("sll", FMT_RD_RT_SA);
        case 0x01:
            return coprocessor_unusable((field_rt(raw) & 1u) ? "movt" : "movf",
                                        FMT_RD_RS, 1);
        case 0x02:
            if (field_rs(raw) == 1u) return valid("rotr", FMT_RD_RT_SA);
            if (field_rs(raw) != 0u) return reserved();
            return valid("srl", FMT_RD_RT_SA);
        case 0x03: return valid("sra", FMT_RD_RT_SA);
        case 0x04: return valid("sllv", FMT_RD_RT_RS);
        case 0x06:
            if (field_sa(raw) == 1u) return valid("rotrv", FMT_RD_RT_RS);
            if (field_sa(raw) != 0u) return reserved();
            return valid("srlv", FMT_RD_RT_RS);
        case 0x07: return valid("srav", FMT_RD_RT_RS);
        case 0x08: return valid(field_sa(raw) == 0x10u ? "jr.hb" : "jr", FMT_RS);
        case 0x09: return valid(field_sa(raw) == 0x10u ? "jalr.hb" : "jalr", FMT_RD_RS);
        case 0x0a: return valid("movz", FMT_RD_RS_RT);
        case 0x0b: return valid("movn", FMT_RD_RS_RT);
        case 0x0c: return valid("syscall", FMT_CODE20);
        case 0x0d: return valid("break", FMT_CODE20);
        case 0x0f: return valid("sync", FMT_SYNC);
        case 0x10: return valid("mfhi", FMT_RD);
        case 0x11: return valid("mthi", FMT_RS);
        case 0x12: return valid("mflo", FMT_RD);
        case 0x13: return valid("mtlo", FMT_RS);
        case 0x18: return valid("mult", FMT_RS_RT);
        case 0x19: return valid("multu", FMT_RS_RT);
        case 0x1a: return valid("div", FMT_RS_RT);
        case 0x1b: return valid("divu", FMT_RS_RT);
        case 0x20: return valid("add", FMT_RD_RS_RT);
        case 0x21: return valid("addu", FMT_RD_RS_RT);
        case 0x22: return valid("sub", FMT_RD_RS_RT);
        case 0x23: return valid("subu", FMT_RD_RS_RT);
        case 0x24: return valid("and", FMT_RD_RS_RT);
        case 0x25: return valid("or", FMT_RD_RS_RT);
        case 0x26: return valid("xor", FMT_RD_RS_RT);
        case 0x27: return valid("nor", FMT_RD_RS_RT);
        case 0x2a: return valid("slt", FMT_RD_RS_RT);
        case 0x2b: return valid("sltu", FMT_RD_RS_RT);
        case 0x30: return valid("tge", FMT_TRAP_RS_RT);
        case 0x31: return valid("tgeu", FMT_TRAP_RS_RT);
        case 0x32: return valid("tlt", FMT_TRAP_RS_RT);
        case 0x33: return valid("tltu", FMT_TRAP_RS_RT);
        case 0x34: return valid("teq", FMT_TRAP_RS_RT);
        case 0x36: return valid("tne", FMT_TRAP_RS_RT);
        default: return reserved();
    }
}

static DecodedInstruction decode_regimm(uint32_t raw) {
    switch (field_rt(raw)) {
        case 0x00: return valid("bltz", FMT_BRANCH_RS);
        case 0x01: return valid("bgez", FMT_BRANCH_RS);
        case 0x02: return valid("bltzl", FMT_BRANCH_RS);
        case 0x03: return valid("bgezl", FMT_BRANCH_RS);
        case 0x08: return valid("tgei", FMT_TRAP_RS_IMM);
        case 0x09: return valid("tgeiu", FMT_TRAP_RS_IMM);
        case 0x0a: return valid("tlti", FMT_TRAP_RS_IMM);
        case 0x0b: return valid("tltiu", FMT_TRAP_RS_IMM);
        case 0x0c: return valid("teqi", FMT_TRAP_RS_IMM);
        case 0x0e: return valid("tnei", FMT_TRAP_RS_IMM);
        case 0x10: return valid("bltzal", FMT_BRANCH_RS);
        case 0x11: return valid("bgezal", FMT_BRANCH_RS);
        case 0x12: return valid("bltzall", FMT_BRANCH_RS);
        case 0x13: return valid("bgezall", FMT_BRANCH_RS);
        case 0x1f: return valid("synci", FMT_MEM_BASE);
        default: return reserved();
    }
}

static DecodedInstruction decode_special2(uint32_t raw) {
    switch (field_fn(raw)) {
        case 0x00: return valid("madd", FMT_RS_RT);
        case 0x01: return valid("maddu", FMT_RS_RT);
        case 0x02: return valid("mul", FMT_RD_RS_RT);
        case 0x05: return valid("msub", FMT_RS_RT);
        case 0x06: return valid("msubu", FMT_RS_RT);
        case 0x20: return valid("clz", FMT_RD_RS);
        case 0x21: return valid("clo", FMT_RD_RS);
        default: return reserved();
    }
}

static DecodedInstruction decode_bshfl(uint32_t raw) {
    switch (field_sa(raw)) {
        case 0x02: return valid("wsbh", FMT_RD_RT);
        case 0x10: return valid("seb", FMT_RD_RT);
        case 0x18: return valid("seh", FMT_RD_RT);
        default: return reserved();
    }
}

static DecodedInstruction decode_special3(uint32_t raw) {
    unsigned pos;
    unsigned size;

    switch (field_fn(raw)) {
        case 0x00:
            pos = field_sa(raw);
            size = field_rd(raw) + 1u;
            return pos + size <= 32u ? valid("ext", FMT_EXT) : reserved();
        case 0x04:
            return field_rd(raw) >= field_sa(raw)
                       ? valid("ins", FMT_INS)
                       : reserved();
        case 0x20: return decode_bshfl(raw);
        case 0x3b: return valid("rdhwr", FMT_RDHWR);
        default: return reserved();
    }
}

static DecodedInstruction decode_cop0(uint32_t raw) {
    unsigned rs = field_rs(raw);

    if ((raw >> 25) & 1u) {
        switch (field_fn(raw)) {
            case 0x01: return valid("tlbr", FMT_NONE);
            case 0x02: return valid("tlbwi", FMT_NONE);
            case 0x06: return valid("tlbwr", FMT_NONE);
            case 0x08: return valid("tlbp", FMT_NONE);
            case 0x18: return valid("eret", FMT_NONE);
            case 0x1f:
                return coprocessor_unusable("deret", FMT_NONE, 0);
            case 0x20:
                return coprocessor_unusable("wait", FMT_NONE, 0);
            default: return reserved();
        }
    }

    switch (rs) {
        case 0x00: return valid("mfc0", FMT_CP0_MOVE);
        case 0x04: return valid("mtc0", FMT_CP0_MOVE);
        case 0x0a:
            return coprocessor_unusable("rdpgpr", FMT_PGPR, 0);
        case 0x0b:
            if (field_rd(raw) != 12u) return reserved();
            return valid((raw & 0x20u) ? "ei" : "di", FMT_DI_EI);
        case 0x0e:
            return coprocessor_unusable("wrpgpr", FMT_PGPR, 0);
        default: return reserved();
    }
}

static DecodedInstruction decode(uint32_t raw) {
    switch (raw >> 26) {
        case 0x00: return decode_special(raw);
        case 0x01: return decode_regimm(raw);
        case 0x02: return valid("j", FMT_JUMP);
        case 0x03: return valid("jal", FMT_JUMP);
        case 0x04: return valid("beq", FMT_BRANCH_RS_RT);
        case 0x05: return valid("bne", FMT_BRANCH_RS_RT);
        case 0x06: return valid("blez", FMT_BRANCH_RS);
        case 0x07: return valid("bgtz", FMT_BRANCH_RS);
        case 0x08: return valid("addi", FMT_RT_RS_SIGNED);
        case 0x09: return valid("addiu", FMT_RT_RS_SIGNED);
        case 0x0a: return valid("slti", FMT_RT_RS_SIGNED);
        case 0x0b: return valid("sltiu", FMT_RT_RS_SIGNED);
        case 0x0c: return valid("andi", FMT_RT_RS_UNSIGNED);
        case 0x0d: return valid("ori", FMT_RT_RS_UNSIGNED);
        case 0x0e: return valid("xori", FMT_RT_RS_UNSIGNED);
        case 0x0f: return valid("lui", FMT_RT_UNSIGNED);
        case 0x10: return decode_cop0(raw);
        case 0x11:
            return coprocessor_unusable("coprocessor-unusable", FMT_RAW, 1);
        case 0x12:
            return coprocessor_unusable("coprocessor-unusable", FMT_RAW, 2);
        case 0x13:
            return coprocessor_unusable("coprocessor-unusable", FMT_RAW, 1);
        case 0x14: return valid("beql", FMT_BRANCH_RS_RT);
        case 0x15: return valid("bnel", FMT_BRANCH_RS_RT);
        case 0x16: return valid("blezl", FMT_BRANCH_RS);
        case 0x17: return valid("bgtzl", FMT_BRANCH_RS);
        case 0x1c: return decode_special2(raw);
        case 0x1d: return valid("jalx", FMT_JUMP);
        case 0x1f: return decode_special3(raw);
        case 0x20: return valid("lb", FMT_MEM_GPR);
        case 0x21: return valid("lh", FMT_MEM_GPR);
        case 0x22: return valid("lwl", FMT_MEM_GPR);
        case 0x23: return valid("lw", FMT_MEM_GPR);
        case 0x24: return valid("lbu", FMT_MEM_GPR);
        case 0x25: return valid("lhu", FMT_MEM_GPR);
        case 0x26: return valid("lwr", FMT_MEM_GPR);
        case 0x28: return valid("sb", FMT_MEM_GPR);
        case 0x29: return valid("sh", FMT_MEM_GPR);
        case 0x2a: return valid("swl", FMT_MEM_GPR);
        case 0x2b: return valid("sw", FMT_MEM_GPR);
        case 0x2e: return valid("swr", FMT_MEM_GPR);
        case 0x2f: return valid("cache", FMT_CACHE);
        case 0x30: return valid("ll", FMT_MEM_GPR);
        case 0x31:
            return coprocessor_unusable("lwc1", FMT_MEM_FPR, 1);
        case 0x32:
            return coprocessor_unusable("lwc2", FMT_MEM_COP, 2);
        case 0x33: return valid("pref", FMT_CACHE);
        case 0x35:
            return coprocessor_unusable("ldc1", FMT_MEM_FPR, 1);
        case 0x36:
            return coprocessor_unusable("ldc2", FMT_MEM_COP, 2);
        case 0x38: return valid("sc", FMT_MEM_GPR);
        case 0x39:
            return coprocessor_unusable("swc1", FMT_MEM_FPR, 1);
        case 0x3a:
            return coprocessor_unusable("swc2", FMT_MEM_COP, 2);
        case 0x3d:
            return coprocessor_unusable("sdc1", FMT_MEM_FPR, 1);
        case 0x3e:
            return coprocessor_unusable("sdc2", FMT_MEM_COP, 2);
        default: return reserved();
    }
}

const char *mips_mnemonic(uint32_t raw) {
    return decode(raw).mnemonic;
}

MipsDecodeKind mips_decode_kind(uint32_t raw) {
    return decode(raw).kind;
}

static uint32_t branch_target(uint32_t pc, uint32_t raw) {
    int32_t displacement = (int32_t)(int16_t)(raw & 0xffffu) * 4;
    return pc + 4u + (uint32_t)displacement;
}

static uint32_t jump_target(uint32_t pc, uint32_t raw) {
    return ((pc + 4u) & 0xf0000000u) | ((raw & 0x03ffffffu) << 2);
}

void mips_disassemble(uint32_t pc, uint32_t raw,
                      char *buf, size_t buf_size) {
    DecodedInstruction decoded;
    char text[160];
    unsigned rs;
    unsigned rt;
    unsigned rd;
    unsigned sa;
    int immediate;
    unsigned code;

    if (buf == NULL || buf_size == 0u) return;

    decoded = decode(raw);
    rs = field_rs(raw);
    rt = field_rt(raw);
    rd = field_rd(raw);
    sa = field_sa(raw);
    immediate = (int)(int16_t)(raw & 0xffffu);
    text[0] = '\0';

    switch (decoded.format) {
        case FMT_RAW:
            (void)snprintf(text, sizeof(text), ".word 0x%08x", raw);
            break;
        case FMT_NONE:
            (void)snprintf(text, sizeof(text), "%s", decoded.mnemonic);
            break;
        case FMT_JUMP:
            (void)snprintf(text, sizeof(text), "%s 0x%08x", decoded.mnemonic,
                           jump_target(pc, raw));
            break;
        case FMT_BRANCH_RS_RT:
            (void)snprintf(text, sizeof(text), "%s %s, %s, 0x%08x",
                           decoded.mnemonic, gpr_names[rs], gpr_names[rt],
                           branch_target(pc, raw));
            break;
        case FMT_BRANCH_RS:
            (void)snprintf(text, sizeof(text), "%s %s, 0x%08x",
                           decoded.mnemonic, gpr_names[rs],
                           branch_target(pc, raw));
            break;
        case FMT_RT_RS_SIGNED:
            (void)snprintf(text, sizeof(text), "%s %s, %s, %d",
                           decoded.mnemonic, gpr_names[rt], gpr_names[rs],
                           immediate);
            break;
        case FMT_RT_RS_UNSIGNED:
            (void)snprintf(text, sizeof(text), "%s %s, %s, 0x%04x",
                           decoded.mnemonic, gpr_names[rt], gpr_names[rs],
                           raw & 0xffffu);
            break;
        case FMT_RT_UNSIGNED:
            (void)snprintf(text, sizeof(text), "%s %s, 0x%04x",
                           decoded.mnemonic, gpr_names[rt], raw & 0xffffu);
            break;
        case FMT_MEM_GPR:
            (void)snprintf(text, sizeof(text), "%s %s, %d(%s)",
                           decoded.mnemonic, gpr_names[rt], immediate,
                           gpr_names[rs]);
            break;
        case FMT_MEM_BASE:
            (void)snprintf(text, sizeof(text), "%s %d(%s)",
                           decoded.mnemonic, immediate, gpr_names[rs]);
            break;
        case FMT_MEM_FPR:
            (void)snprintf(text, sizeof(text), "%s $f%u, %d(%s)",
                           decoded.mnemonic, rt, immediate, gpr_names[rs]);
            break;
        case FMT_MEM_COP:
            (void)snprintf(text, sizeof(text), "%s $%u, %d(%s)",
                           decoded.mnemonic, rt, immediate, gpr_names[rs]);
            break;
        case FMT_CACHE:
            (void)snprintf(text, sizeof(text), "%s 0x%x, %d(%s)",
                           decoded.mnemonic, rt, immediate, gpr_names[rs]);
            break;
        case FMT_RD_RT_SA:
            (void)snprintf(text, sizeof(text), "%s %s, %s, %u",
                           decoded.mnemonic, gpr_names[rd], gpr_names[rt], sa);
            break;
        case FMT_RD_RT_RS:
            (void)snprintf(text, sizeof(text), "%s %s, %s, %s",
                           decoded.mnemonic, gpr_names[rd], gpr_names[rt],
                           gpr_names[rs]);
            break;
        case FMT_RS:
            (void)snprintf(text, sizeof(text), "%s %s", decoded.mnemonic,
                           gpr_names[rs]);
            break;
        case FMT_RD_RS:
            (void)snprintf(text, sizeof(text), "%s %s, %s", decoded.mnemonic,
                           gpr_names[rd], gpr_names[rs]);
            break;
        case FMT_RD_RS_RT:
            (void)snprintf(text, sizeof(text), "%s %s, %s, %s",
                           decoded.mnemonic, gpr_names[rd], gpr_names[rs],
                           gpr_names[rt]);
            break;
        case FMT_RS_RT:
            (void)snprintf(text, sizeof(text), "%s %s, %s", decoded.mnemonic,
                           gpr_names[rs], gpr_names[rt]);
            break;
        case FMT_TRAP_RS_RT:
            code = (raw >> 6) & 0x3ffu;
            if (code == 0u) {
                (void)snprintf(text, sizeof(text), "%s %s, %s",
                               decoded.mnemonic, gpr_names[rs], gpr_names[rt]);
            } else {
                (void)snprintf(text, sizeof(text), "%s %s, %s, 0x%x",
                               decoded.mnemonic, gpr_names[rs], gpr_names[rt],
                               code);
            }
            break;
        case FMT_CODE20:
            code = (raw >> 6) & 0xfffffu;
            if (code == 0u) {
                (void)snprintf(text, sizeof(text), "%s", decoded.mnemonic);
            } else {
                (void)snprintf(text, sizeof(text), "%s 0x%x",
                               decoded.mnemonic, code);
            }
            break;
        case FMT_SYNC:
            if (sa == 0u) {
                (void)snprintf(text, sizeof(text), "%s", decoded.mnemonic);
            } else {
                (void)snprintf(text, sizeof(text), "%s 0x%x",
                               decoded.mnemonic, sa);
            }
            break;
        case FMT_RD:
            (void)snprintf(text, sizeof(text), "%s %s", decoded.mnemonic,
                           gpr_names[rd]);
            break;
        case FMT_TRAP_RS_IMM:
            (void)snprintf(text, sizeof(text), "%s %s, %d", decoded.mnemonic,
                           gpr_names[rs], immediate);
            break;
        case FMT_EXT:
            (void)snprintf(text, sizeof(text), "%s %s, %s, %u, %u",
                           decoded.mnemonic, gpr_names[rt], gpr_names[rs], sa,
                           rd + 1u);
            break;
        case FMT_INS:
            (void)snprintf(text, sizeof(text), "%s %s, %s, %u, %u",
                           decoded.mnemonic, gpr_names[rt], gpr_names[rs], sa,
                           rd - sa + 1u);
            break;
        case FMT_RD_RT:
            (void)snprintf(text, sizeof(text), "%s %s, %s", decoded.mnemonic,
                           gpr_names[rd], gpr_names[rt]);
            break;
        case FMT_RDHWR:
            (void)snprintf(text, sizeof(text), "%s %s, $%u",
                           decoded.mnemonic, gpr_names[rt], rd);
            break;
        case FMT_CP0_MOVE:
            (void)snprintf(text, sizeof(text), "%s %s, $%u, %u",
                           decoded.mnemonic, gpr_names[rt], rd, raw & 7u);
            break;
        case FMT_PGPR:
            (void)snprintf(text, sizeof(text), "%s %s, %s",
                           decoded.mnemonic, gpr_names[rt], gpr_names[rd]);
            break;
        case FMT_DI_EI:
            if (rt == 0u) {
                (void)snprintf(text, sizeof(text), "%s", decoded.mnemonic);
            } else {
                (void)snprintf(text, sizeof(text), "%s %s", decoded.mnemonic,
                               gpr_names[rt]);
            }
            break;
    }

    if (decoded.kind == MIPS_DECODE_RESERVED) {
        (void)snprintf(buf, buf_size, "%s # reserved instruction", text);
    } else if (decoded.kind == MIPS_DECODE_COPROCESSOR_UNUSABLE) {
        (void)snprintf(buf, buf_size, "%s # coprocessor %u unusable", text,
                       decoded.coprocessor);
    } else {
        (void)snprintf(buf, buf_size, "%s", text);
    }
}
