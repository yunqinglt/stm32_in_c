#include "console.h"

#include "disasm.h"
#include "format.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CONSOLE_MAX_TOKENS 4u
#define TLB_ENTRY_COUNT 64u

static const char *const gpr_names[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

typedef struct {
    const char *name;
    unsigned reg;
    unsigned sel;
} Cp0Name;

static const Cp0Name cp0_names[] = {
    {"index", 0, 0},       {"random", 1, 0},
    {"entrylo0", 2, 0},   {"entrylo1", 3, 0},
    {"context", 4, 0},    {"pagemask", 5, 0},
    {"wired", 6, 0},      {"badvaddr", 8, 0},
    {"count", 9, 0},      {"entryhi", 10, 0},
    {"compare", 11, 0},   {"status", 12, 0},
    {"intctl", 12, 1},    {"srsctl", 12, 2},
    {"cause", 13, 0},     {"epc", 14, 0},
    {"prid", 15, 0},      {"ebase", 15, 1},
    {"config", 16, 0},    {"config0", 16, 0},
    {"config1", 16, 1},   {"config2", 16, 2},
    {"config3", 16, 3},   {"config4", 16, 4},
    {"config5", 16, 5},   {"lladdr", 17, 0},
    {"debug", 23, 0},     {"depc", 24, 0},
    {"errctl", 26, 0},    {"errorepc", 30, 0},
    {"desave", 31, 0},
};

typedef enum {
    REGISTER_GPR,
    REGISTER_PC,
    REGISTER_NEXT_PC,
    REGISTER_HI,
    REGISTER_LO,
    REGISTER_CP0,
} RegisterKind;

typedef struct {
    RegisterKind kind;
    unsigned reg;
    unsigned sel;
    const char *name;
} RegisterRef;

static size_t text_length(const char *text) {
    size_t length = 0;
    if (!text) return 0;
    while (text[length] != '\0') ++length;
    return length;
}

static char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z') return (char)(value + ('a' - 'A'));
    return value;
}

static bool text_equal(const char *left, const char *right) {
    size_t index = 0;
    if (!left || !right) return false;
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) return false;
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

static bool is_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\v' || value == '\f';
}

static void emit_bytes(mipsel_console_t *console, const char *bytes,
                       size_t length) {
    if (console && console->initialized && console->config.output &&
        bytes && length != 0) {
        console->config.output(console->config.output_opaque, bytes, length);
    }
}

static void emit_text(mipsel_console_t *console, const char *text) {
    emit_bytes(console, text, text_length(text));
}

static void emit_line(mipsel_console_t *console, const char *text) {
    emit_text(console, text);
    emit_bytes(console, "\r\n", 2);
}

#define EMIT_FORMAT(console_, ...) do {                                      \
    char formatted_[MIPSEL_EMU_CONSOLE_OUTPUT_SIZE];                         \
    (void)mipsel_snprintf(formatted_, sizeof(formatted_), __VA_ARGS__);       \
    emit_line((console_), formatted_);                                        \
} while (0)

static bool parse_u32(const char *text, uint32_t *value) {
    uint32_t result = 0;
    unsigned base = 10;
    size_t index = 0;
    bool any = false;

    if (!text || !value || text[0] == '\0') return false;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        index = 2;
    }
    for (; text[index] != '\0'; ++index) {
        unsigned digit;
        char current = text[index];

        if (current >= '0' && current <= '9') {
            digit = (unsigned)(current - '0');
        } else if (current >= 'a' && current <= 'f') {
            digit = (unsigned)(current - 'a') + 10u;
        } else if (current >= 'A' && current <= 'F') {
            digit = (unsigned)(current - 'A') + 10u;
        } else {
            return false;
        }
        if (digit >= base || result > (UINT32_MAX - digit) / base) return false;
        result = result * base + digit;
        any = true;
    }
    if (!any) return false;
    *value = result;
    return true;
}

static bool parse_decimal_component(const char *text, size_t *used,
                                    unsigned *value) {
    size_t index = 0;
    unsigned result = 0;

    if (!text || !used || !value || text[0] < '0' || text[0] > '9')
        return false;
    while (text[index] >= '0' && text[index] <= '9') {
        unsigned digit = (unsigned)(text[index] - '0');
        if (result > (255u - digit) / 10u) return false;
        result = result * 10u + digit;
        ++index;
    }
    *used = index;
    *value = result;
    return true;
}

static bool resolve_register(const char *name, RegisterRef *reference) {
    const char *plain = name;
    size_t used;
    unsigned number;

    if (!name || !reference) return false;
    if (*plain == '$') ++plain;

    for (unsigned index = 0; index < 32u; ++index) {
        if (text_equal(plain, gpr_names[index])) {
            *reference = (RegisterRef){REGISTER_GPR, index, 0,
                                       gpr_names[index]};
            return true;
        }
    }

    if ((plain[0] == 'r' || plain[0] == 'R') &&
        parse_decimal_component(plain + 1, &used, &number) &&
        plain[1 + used] == '\0' && number < 32u) {
        *reference = (RegisterRef){REGISTER_GPR, number, 0,
                                   gpr_names[number]};
        return true;
    }
    if (parse_decimal_component(plain, &used, &number) &&
        plain[used] == '\0' && number < 32u) {
        *reference = (RegisterRef){REGISTER_GPR, number, 0,
                                   gpr_names[number]};
        return true;
    }

    if (text_equal(plain, "pc")) {
        *reference = (RegisterRef){REGISTER_PC, 0, 0, "pc"};
        return true;
    }
    if (text_equal(plain, "nextpc") || text_equal(plain, "next_pc")) {
        *reference = (RegisterRef){REGISTER_NEXT_PC, 0, 0, "next_pc"};
        return true;
    }
    if (text_equal(plain, "hi")) {
        *reference = (RegisterRef){REGISTER_HI, 0, 0, "hi"};
        return true;
    }
    if (text_equal(plain, "lo")) {
        *reference = (RegisterRef){REGISTER_LO, 0, 0, "lo"};
        return true;
    }

    for (size_t index = 0;
         index < sizeof(cp0_names) / sizeof(cp0_names[0]); ++index) {
        if (text_equal(plain, cp0_names[index].name)) {
            *reference = (RegisterRef){REGISTER_CP0, cp0_names[index].reg,
                                       cp0_names[index].sel,
                                       cp0_names[index].name};
            return true;
        }
    }

    if (ascii_lower(plain[0]) == 'c' && ascii_lower(plain[1]) == 'p' &&
        plain[2] == '0' && plain[3] == '.' &&
        parse_decimal_component(plain + 4, &used, &number) && number < 32u) {
        const char *selection = plain + 4 + used;
        unsigned select = 0;
        size_t select_used = 0;

        if (*selection == '.') {
            if (!parse_decimal_component(selection + 1, &select_used, &select) ||
                selection[1 + select_used] != '\0' || select > 7u) {
                return false;
            }
        } else if (*selection != '\0') {
            return false;
        }
        *reference = (RegisterRef){REGISTER_CP0, number, select, NULL};
        return true;
    }
    return false;
}

static uint32_t register_value(const Registers *state,
                               const RegisterRef *reference) {
    switch (reference->kind) {
        case REGISTER_GPR: return state->gpr[reference->reg];
        case REGISTER_PC: return state->pc;
        case REGISTER_NEXT_PC: return state->next_pc;
        case REGISTER_HI: return state->hi;
        case REGISTER_LO: return state->lo;
        case REGISTER_CP0: return state->cp0.regs[reference->reg][reference->sel];
        default: return 0;
    }
}

static void format_register_name(const RegisterRef *reference, char *output,
                                 size_t output_size) {
    if (reference->kind == REGISTER_GPR) {
        (void)mipsel_snprintf(output, output_size, "$%s", reference->name);
    } else if (reference->kind == REGISTER_CP0 && !reference->name) {
        (void)mipsel_snprintf(output, output_size, "cp0.%u.%u",
                              reference->reg, reference->sel);
    } else {
        (void)mipsel_snprintf(output, output_size, "%s", reference->name);
    }
}

static bool set_register(Registers *state, const RegisterRef *reference,
                         uint32_t value) {
    switch (reference->kind) {
        case REGISTER_GPR:
            if (reference->reg == 0u) return false;
            state->gpr[reference->reg] = value;
            return true;
        case REGISTER_PC:
            state->pc = value;
            state->next_pc = value + 4u;
            state->is_delay_slot = 0;
            state->is_taken = 0;
            state->target_pc = 0;
            state->bds = 0;
            state->exception_pending = 0;
            state->ll_bit = 0;
            state->ll_addr = 0;
            return true;
        case REGISTER_NEXT_PC:
            state->next_pc = value;
            state->ll_bit = 0;
            state->ll_addr = 0;
            return true;
        case REGISTER_HI:
            state->hi = value;
            return true;
        case REGISTER_LO:
            state->lo = value;
            return true;
        case REGISTER_CP0:
            return mipsel_cp0_write(state, reference->reg, reference->sel,
                                    value);
        default:
            return false;
    }
}

static void emit_all_registers(mipsel_console_t *console) {
    Registers *state = console->config.registers;

    for (unsigned index = 0; index < 32u; index += 4u) {
        EMIT_FORMAT(console,
                    "$%s=%08x  $%s=%08x  $%s=%08x  $%s=%08x",
                    gpr_names[index], state->gpr[index],
                    gpr_names[index + 1u], state->gpr[index + 1u],
                    gpr_names[index + 2u], state->gpr[index + 2u],
                    gpr_names[index + 3u], state->gpr[index + 3u]);
    }
    EMIT_FORMAT(console, "pc=%08x  next_pc=%08x  hi=%08x  lo=%08x",
                state->pc, state->next_pc, state->hi, state->lo);
    EMIT_FORMAT(console, "status=%08x  cause=%08x  epc=%08x  badvaddr=%08x",
                state->cp0.regs[12][0], state->cp0.regs[13][0],
                state->cp0.regs[14][0], state->cp0.regs[8][0]);
    EMIT_FORMAT(console, "count=%08x  compare=%08x  entryhi=%08x  pagemask=%08x",
                state->cp0.regs[9][0], state->cp0.regs[11][0],
                state->cp0.regs[10][0], state->cp0.regs[5][0]);
}

static mipsel_console_result_t command_register(mipsel_console_t *console,
                                                 char **tokens,
                                                 size_t count) {
    Registers *state = console->config.registers;
    RegisterRef reference;
    char label[32];
    uint32_t value;

    if (count == 1u) {
        emit_all_registers(console);
        return MIPSEL_CONSOLE_OK;
    }
    if (count > 3u) {
        emit_line(console, "usage: reg [register [value]]");
        return MIPSEL_CONSOLE_ERROR;
    }
    if (!resolve_register(tokens[1], &reference)) {
        EMIT_FORMAT(console, "error: unknown register '%s'", tokens[1]);
        return MIPSEL_CONSOLE_ERROR;
    }
    format_register_name(&reference, label, sizeof(label));
    if (count == 2u) {
        EMIT_FORMAT(console, "%s = 0x%08x", label,
                    register_value(state, &reference));
        return MIPSEL_CONSOLE_OK;
    }
    if (!parse_u32(tokens[2], &value)) {
        EMIT_FORMAT(console, "error: invalid 32-bit value '%s'", tokens[2]);
        return MIPSEL_CONSOLE_ERROR;
    }
    if (!set_register(state, &reference, value)) {
        EMIT_FORMAT(console, "error: register %s is read-only", label);
        return MIPSEL_CONSOLE_ERROR;
    }
    EMIT_FORMAT(console, "%s <- 0x%08x", label,
                register_value(state, &reference));
    return MIPSEL_CONSOLE_STATE_CHANGED;
}

static void emit_tlb_entry(mipsel_console_t *console, unsigned index) {
    const Registers *state = console->config.registers;
    EMIT_FORMAT(console,
                "tlb[%02u] hi=%08x lo0=%08x lo1=%08x mask=%08x",
                index, state->tlb[index].entryhi,
                state->tlb[index].entrylo0,
                state->tlb[index].entrylo1, state->tlb[index].pmask);
}

static mipsel_console_result_t command_tlb(mipsel_console_t *console,
                                           char **tokens, size_t count) {
    uint32_t index;

    if (count > 2u) {
        emit_line(console, "usage: tlb [index]");
        return MIPSEL_CONSOLE_ERROR;
    }
    if (count == 1u) {
        for (unsigned current = 0; current < TLB_ENTRY_COUNT; ++current)
            emit_tlb_entry(console, current);
        return MIPSEL_CONSOLE_OK;
    }
    if (!parse_u32(tokens[1], &index) || index >= TLB_ENTRY_COUNT) {
        EMIT_FORMAT(console, "error: TLB index must be 0..%u",
                    TLB_ENTRY_COUNT - 1u);
        return MIPSEL_CONSOLE_ERROR;
    }
    emit_tlb_entry(console, (unsigned)index);
    return MIPSEL_CONSOLE_OK;
}

static const char *translation_error(uint32_t reason) {
    switch (reason) {
        case 2: return "TLB refill";
        case 3: return "TLB invalid";
        case 4: return "TLB modified";
        default: return "translation failure";
    }
}

static mipsel_console_result_t command_translate(mipsel_console_t *console,
                                                 char **tokens,
                                                 size_t count) {
    uint32_t address;
    Result translated;

    if (count != 2u) {
        emit_line(console, "usage: translate <virtual-address>");
        return MIPSEL_CONSOLE_ERROR;
    }
    if (!parse_u32(tokens[1], &address)) {
        EMIT_FORMAT(console, "error: invalid virtual address '%s'", tokens[1]);
        return MIPSEL_CONSOLE_ERROR;
    }
    translated = pfn_translate(address, console->config.registers, 0);
    if (!TEST_RESULT(translated)) {
        EMIT_FORMAT(console, "VA 0x%08x: %s", address,
                    translation_error(translated.value.reason));
        return MIPSEL_CONSOLE_ERROR;
    }
    EMIT_FORMAT(console, "VA 0x%08x -> PA 0x%08x", address,
                translated.value.ok);
    return MIPSEL_CONSOLE_OK;
}

static bool memory_command(const char *name, bool *write, unsigned *width) {
    if (text_equal(name, "mrb")) { *write = false; *width = 1; return true; }
    if (text_equal(name, "mrh")) { *write = false; *width = 2; return true; }
    if (text_equal(name, "mrw")) { *write = false; *width = 4; return true; }
    if (text_equal(name, "mwb")) { *write = true; *width = 1; return true; }
    if (text_equal(name, "mwh")) { *write = true; *width = 2; return true; }
    if (text_equal(name, "mww")) { *write = true; *width = 4; return true; }
    return false;
}

static mipsel_console_result_t command_memory(mipsel_console_t *console,
                                              char **tokens, size_t count,
                                              bool write, unsigned width) {
    uint32_t address;
    uint32_t value;
    uint32_t maximum = width == 1u ? UINT32_C(0xff) :
                       width == 2u ? UINT32_C(0xffff) : UINT32_MAX;

    if ((!write && count != 2u) || (write && count != 3u)) {
        emit_line(console, write ? "usage: mwb|mwh|mww <pa> <value>" :
                                  "usage: mrb|mrh|mrw <pa>");
        return MIPSEL_CONSOLE_ERROR;
    }
    if (!parse_u32(tokens[1], &address)) {
        EMIT_FORMAT(console, "error: invalid physical address '%s'", tokens[1]);
        return MIPSEL_CONSOLE_ERROR;
    }

    if (!write) {
        if (!console->config.bus_read) {
            emit_line(console, "error: physical bus read is unavailable");
            return MIPSEL_CONSOLE_ERROR;
        }
        if (!console->config.bus_read(console->config.target_opaque, address,
                                      width, &value)) {
            EMIT_FORMAT(console, "error: cannot read PA 0x%08x", address);
            return MIPSEL_CONSOLE_ERROR;
        }
        if (width == 1u) {
            EMIT_FORMAT(console, "%s 0x%08x = 0x%02x", tokens[0], address,
                        value);
        } else if (width == 2u) {
            EMIT_FORMAT(console, "%s 0x%08x = 0x%04x", tokens[0], address,
                        value);
        } else {
            EMIT_FORMAT(console, "%s 0x%08x = 0x%08x", tokens[0], address,
                        value);
        }
        return MIPSEL_CONSOLE_OK;
    }

    if (!parse_u32(tokens[2], &value) || value > maximum) {
        EMIT_FORMAT(console, "error: value '%s' does not fit %u bits",
                    tokens[2], width * 8u);
        return MIPSEL_CONSOLE_ERROR;
    }
    if (!console->config.bus_write) {
        emit_line(console, "error: physical bus write is unavailable");
        return MIPSEL_CONSOLE_ERROR;
    }
    if (!console->config.bus_write(console->config.target_opaque, address,
                                   width, value)) {
        EMIT_FORMAT(console, "error: cannot write PA 0x%08x", address);
        return MIPSEL_CONSOLE_ERROR;
    }
    console->config.registers->ll_bit = 0;
    console->config.registers->ll_addr = 0;
    if (width == 1u) {
        EMIT_FORMAT(console, "%s 0x%08x <- 0x%02x", tokens[0], address,
                    value);
    } else if (width == 2u) {
        EMIT_FORMAT(console, "%s 0x%08x <- 0x%04x", tokens[0], address,
                    value);
    } else {
        EMIT_FORMAT(console, "%s 0x%08x <- 0x%08x", tokens[0], address,
                    value);
    }
    return MIPSEL_CONSOLE_STATE_CHANGED;
}

static mipsel_console_result_t command_disasm(mipsel_console_t *console,
                                              char **tokens, size_t count) {
    Registers *state = console->config.registers;
    uint32_t raw;
    char disassembly[160];

    if (count > 2u) {
        emit_line(console, "usage: disasm [instruction-word]");
        return MIPSEL_CONSOLE_ERROR;
    }
    if (count == 2u) {
        if (!parse_u32(tokens[1], &raw)) {
            EMIT_FORMAT(console, "error: invalid instruction word '%s'",
                        tokens[1]);
            return MIPSEL_CONSOLE_ERROR;
        }
    } else {
        Result translated;

        if ((state->pc & 3u) != 0u) {
            EMIT_FORMAT(console, "error: PC 0x%08x is not word-aligned",
                        state->pc);
            return MIPSEL_CONSOLE_ERROR;
        }
        translated = pfn_translate(state->pc, state, 0);
        if (!TEST_RESULT(translated)) {
            EMIT_FORMAT(console, "PC 0x%08x: %s", state->pc,
                        translation_error(translated.value.reason));
            return MIPSEL_CONSOLE_ERROR;
        }
        if (!console->config.bus_read ||
            !console->config.bus_read(console->config.target_opaque,
                                      translated.value.ok, 4, &raw)) {
            EMIT_FORMAT(console, "error: cannot read instruction at PA 0x%08x",
                        translated.value.ok);
            return MIPSEL_CONSOLE_ERROR;
        }
    }
    mips_disassemble(state->pc, raw, disassembly, sizeof(disassembly));
    EMIT_FORMAT(console, "0x%08x: 0x%08x  %s", state->pc, raw, disassembly);
    return MIPSEL_CONSOLE_OK;
}

static void emit_help(mipsel_console_t *console) {
    emit_line(console, "help                         show this command list");
    emit_line(console, "tlb [index]                  print one/all TLB entries");
    emit_line(console, "translate <va>               translate VA with current TLB/ASID");
    emit_line(console, "reg [name [value]]           print or set CPU/CP0 registers");
    emit_line(console, "mrb|mrh|mrw <pa>             read 8/16/32-bit physical bus");
    emit_line(console, "mwb|mwh|mww <pa> <value>     write 8/16/32-bit physical bus");
    emit_line(console, "disasm [word]                decode word, or instruction at PC");
    emit_line(console, "numbers use decimal or a 0x-prefixed hexadecimal form");
}

bool mipsel_console_init(mipsel_console_t *console,
                         const mipsel_console_config_t *config) {
    if (!console || !config || !config->registers || !config->halted ||
        !config->output) {
        return false;
    }
    memset(console, 0, sizeof(*console));
    console->config = *config;
    console->initialized = true;
    return true;
}

void mipsel_console_cancel_input(mipsel_console_t *console) {
    if (!console) return;
    console->input_length = 0;
    console->discarded_input = 0;
    console->previous_was_cr = false;
    console->input[0] = '\0';
}

void mipsel_console_reset(mipsel_console_t *console) {
    mipsel_console_cancel_input(console);
}

void mipsel_console_prompt(mipsel_console_t *console) {
    emit_text(console, "mipsel-emu> ");
}

const char *mipsel_console_input(const mipsel_console_t *console) {
    return console && console->initialized ? console->input : "";
}

size_t mipsel_console_input_length(const mipsel_console_t *console) {
    return console && console->initialized ? console->input_length : 0;
}

mipsel_console_result_t mipsel_console_execute(
    mipsel_console_t *console, const char *line, size_t length) {
    char scratch[MIPSEL_EMU_CONSOLE_LINE_SIZE];
    char *tokens[CONSOLE_MAX_TOKENS];
    size_t token_count = 0;
    char *cursor;
    bool write;
    unsigned width;

    if (!console || !console->initialized || (!line && length != 0))
        return MIPSEL_CONSOLE_ERROR;
    if (length >= sizeof(scratch)) {
        emit_line(console, "error: command line is too long");
        return MIPSEL_CONSOLE_ERROR;
    }
    for (size_t index = 0; index < length; ++index) {
        if (line[index] == '\0') {
            emit_line(console, "error: command contains a NUL byte");
            return MIPSEL_CONSOLE_ERROR;
        }
        scratch[index] = line[index];
    }
    scratch[length] = '\0';

    cursor = scratch;
    while (*cursor != '\0') {
        while (is_space(*cursor)) ++cursor;
        if (*cursor == '\0') break;
        if (token_count == CONSOLE_MAX_TOKENS) {
            emit_line(console, "error: too many command arguments");
            return MIPSEL_CONSOLE_ERROR;
        }
        tokens[token_count++] = cursor;
        while (*cursor != '\0' && !is_space(*cursor)) ++cursor;
        if (*cursor != '\0') *cursor++ = '\0';
    }
    if (token_count == 0u) return MIPSEL_CONSOLE_OK;

    if (!console->config.halted(console->config.target_opaque)) {
        emit_line(console, "error: target not halted");
        return MIPSEL_CONSOLE_TARGET_NOT_HALTED;
    }

    if (text_equal(tokens[0], "help") || text_equal(tokens[0], "?")) {
        if (token_count != 1u) {
            emit_line(console, "usage: help");
            return MIPSEL_CONSOLE_ERROR;
        }
        emit_help(console);
        return MIPSEL_CONSOLE_OK;
    }
    if (text_equal(tokens[0], "tlb"))
        return command_tlb(console, tokens, token_count);
    if (text_equal(tokens[0], "translate"))
        return command_translate(console, tokens, token_count);
    if (text_equal(tokens[0], "reg"))
        return command_register(console, tokens, token_count);
    if (memory_command(tokens[0], &write, &width))
        return command_memory(console, tokens, token_count, write, width);
    if (text_equal(tokens[0], "disasm"))
        return command_disasm(console, tokens, token_count);

    EMIT_FORMAT(console, "error: unknown command '%s' (try 'help')", tokens[0]);
    return MIPSEL_CONSOLE_ERROR;
}

static mipsel_console_result_t merge_feed_result(
    mipsel_console_result_t aggregate, mipsel_console_result_t current) {
    if (current == MIPSEL_CONSOLE_NO_COMMAND) return aggregate;
    if (aggregate == MIPSEL_CONSOLE_NO_COMMAND) return current;
    if (aggregate == MIPSEL_CONSOLE_TARGET_NOT_HALTED ||
        current == MIPSEL_CONSOLE_TARGET_NOT_HALTED) {
        return MIPSEL_CONSOLE_TARGET_NOT_HALTED;
    }
    if (aggregate == MIPSEL_CONSOLE_STATE_CHANGED ||
        current == MIPSEL_CONSOLE_STATE_CHANGED) {
        return MIPSEL_CONSOLE_STATE_CHANGED;
    }
    if (aggregate == MIPSEL_CONSOLE_ERROR ||
        current == MIPSEL_CONSOLE_ERROR) {
        return MIPSEL_CONSOLE_ERROR;
    }
    return MIPSEL_CONSOLE_OK;
}

mipsel_console_result_t mipsel_console_feed(
    mipsel_console_t *console, const uint8_t *bytes, size_t length) {
    mipsel_console_result_t aggregate = MIPSEL_CONSOLE_NO_COMMAND;

    if (!console || !console->initialized || (!bytes && length != 0))
        return MIPSEL_CONSOLE_ERROR;
    for (size_t index = 0; index < length; ++index) {
        uint8_t byte = bytes[index];

        if (byte == '\n' && console->previous_was_cr) {
            console->previous_was_cr = false;
            continue;
        }
        console->previous_was_cr = byte == '\r';

        if (byte == '\r' || byte == '\n') {
            if (console->config.flags & MIPSEL_CONSOLE_FLAG_ECHO)
                emit_bytes(console, "\r\n", 2);
            if (console->discarded_input != 0u) {
                emit_line(console, "error: command line is too long");
                aggregate = merge_feed_result(aggregate,
                                              MIPSEL_CONSOLE_ERROR);
            } else {
                aggregate = merge_feed_result(
                    aggregate,
                    mipsel_console_execute(console, console->input,
                                           console->input_length));
            }
            console->input_length = 0;
            console->discarded_input = 0;
            console->input[0] = '\0';
            if (console->config.flags & MIPSEL_CONSOLE_FLAG_PROMPT)
                mipsel_console_prompt(console);
            continue;
        }

        if (byte == 0x08u || byte == 0x7fu) {
            if (console->discarded_input != 0u) {
                --console->discarded_input;
            } else if (console->input_length != 0u) {
                --console->input_length;
                console->input[console->input_length] = '\0';
            } else {
                continue;
            }
            if (console->config.flags & MIPSEL_CONSOLE_FLAG_ECHO)
                emit_bytes(console, "\b \b", 3);
            continue;
        }

        if (byte == 0x15u) {
            console->input_length = 0;
            console->discarded_input = 0;
            console->input[0] = '\0';
            if (console->config.flags & MIPSEL_CONSOLE_FLAG_ECHO) {
                emit_bytes(console, "\r\n", 2);
                if (console->config.flags & MIPSEL_CONSOLE_FLAG_PROMPT)
                    mipsel_console_prompt(console);
            }
            continue;
        }

        if ((byte < 0x20u && byte != '\t') || byte > 0x7eu) {
            if (console->config.flags & MIPSEL_CONSOLE_FLAG_ECHO)
                emit_bytes(console, "\a", 1);
            continue;
        }
        if (console->input_length < sizeof(console->input) - 1u &&
            console->discarded_input == 0u) {
            console->input[console->input_length++] = (char)byte;
            console->input[console->input_length] = '\0';
        } else {
            if (console->discarded_input != SIZE_MAX)
                ++console->discarded_input;
        }
        if (console->config.flags & MIPSEL_CONSOLE_FLAG_ECHO)
            emit_bytes(console, (const char *)&byte, 1);
    }
    return aggregate;
}
