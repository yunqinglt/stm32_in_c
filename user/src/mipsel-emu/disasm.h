#ifndef MIPSEL_EMU_DISASM_H
#define MIPSEL_EMU_DISASM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MIPS_DECODE_VALID = 0,
    MIPS_DECODE_RESERVED,
    MIPS_DECODE_COPROCESSOR_UNUSABLE,
} MipsDecodeKind;

/*
 * Return the canonical mnemonic for RAW.  The returned pointer has static
 * storage duration.  Reserved encodings return "reserved"; an otherwise
 * undecoded coprocessor instruction returns "coprocessor-unusable".
 */
const char *mips_mnemonic(uint32_t raw);

/* Classify an encoding independently of its formatted presentation. */
MipsDecodeKind mips_decode_kind(uint32_t raw);

/*
 * Format one MIPS32 Release 2 instruction.  Branch and jump operands are
 * rendered as absolute virtual addresses using PC.  BUF is always terminated
 * when it is non-null and BUF_SIZE is non-zero.  A null BUF or a zero
 * BUF_SIZE is permitted.
 */
void mips_disassemble(uint32_t pc, uint32_t raw,
                      char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif
