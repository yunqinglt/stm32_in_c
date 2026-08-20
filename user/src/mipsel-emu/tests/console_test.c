#include "console.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MEMORY_SIZE 512u
#define TEST_OUTPUT_SIZE 32768u

#define CHECK(condition) do {                                                \
    if (!(condition)) {                                                      \
        fprintf(stderr, "%s:%d: check failed: %s\n",                      \
                __func__, __LINE__, #condition);                             \
        return __LINE__;                                                     \
    }                                                                        \
} while (0)

typedef struct {
    Registers state;
    mipsel_console_t console;
    bool halted;
    bool fail_read;
    bool fail_write;
    uint8_t memory[TEST_MEMORY_SIZE];
    unsigned read_calls;
    unsigned write_calls;
    uint32_t last_pa;
    uint32_t last_value;
    unsigned last_width;
    char output[TEST_OUTPUT_SIZE];
    size_t output_length;
    bool output_overflow;
} Fixture;

static bool target_halted(void *opaque) {
    return ((Fixture *)opaque)->halted;
}

static bool width_supported(unsigned width) {
    return width == 1u || width == 2u || width == 4u;
}

static bool bus_range_valid(uint32_t pa, unsigned width) {
    return width_supported(width) && width <= TEST_MEMORY_SIZE &&
           pa <= TEST_MEMORY_SIZE - width;
}

static bool bus_read(void *opaque, uint32_t pa, unsigned width,
                     uint32_t *value) {
    Fixture *fixture = opaque;
    uint32_t result = 0;

    ++fixture->read_calls;
    fixture->last_pa = pa;
    fixture->last_width = width;
    if (!value || fixture->fail_read || !bus_range_valid(pa, width))
        return false;
    for (unsigned index = 0; index < width; ++index)
        result |= (uint32_t)fixture->memory[pa + index] << (index * 8u);
    fixture->last_value = result;
    *value = result;
    return true;
}

static bool bus_write(void *opaque, uint32_t pa, unsigned width,
                      uint32_t value) {
    Fixture *fixture = opaque;

    ++fixture->write_calls;
    fixture->last_pa = pa;
    fixture->last_width = width;
    fixture->last_value = value;
    if (fixture->fail_write || !bus_range_valid(pa, width)) return false;
    for (unsigned index = 0; index < width; ++index)
        fixture->memory[pa + index] = (uint8_t)(value >> (index * 8u));
    return true;
}

static void output_write(void *opaque, const char *bytes, size_t length) {
    Fixture *fixture = opaque;
    size_t available;

    if (!bytes || length == 0u) return;
    available = sizeof(fixture->output) - 1u - fixture->output_length;
    if (length > available) {
        fixture->output_overflow = true;
        length = available;
    }
    if (length != 0u) {
        memcpy(fixture->output + fixture->output_length, bytes, length);
        fixture->output_length += length;
    }
    fixture->output[fixture->output_length] = '\0';
}

static bool fixture_init(Fixture *fixture) {
    mipsel_console_config_t config;

    memset(fixture, 0, sizeof(*fixture));
    fixture->halted = true;
    fixture->state.pc = UINT32_C(0x80000000);
    fixture->state.next_pc = UINT32_C(0x80000004);
    config = (mipsel_console_config_t) {
        .registers = &fixture->state,
        .halted = target_halted,
        .bus_read = bus_read,
        .bus_write = bus_write,
        .target_opaque = fixture,
        .output = output_write,
        .output_opaque = fixture,
        .flags = 0,
    };
    return mipsel_console_init(&fixture->console, &config);
}

static void clear_output(Fixture *fixture) {
    fixture->output_length = 0;
    fixture->output_overflow = false;
    fixture->output[0] = '\0';
}

static void clear_bus_log(Fixture *fixture) {
    fixture->read_calls = 0;
    fixture->write_calls = 0;
    fixture->last_pa = 0;
    fixture->last_value = 0;
    fixture->last_width = 0;
}

static mipsel_console_result_t execute(Fixture *fixture, const char *line) {
    clear_output(fixture);
    return mipsel_console_execute(&fixture->console, line, strlen(line));
}

static bool output_is(const Fixture *fixture, const char *expected) {
    return !fixture->output_overflow &&
           fixture->output_length == strlen(expected) &&
           memcmp(fixture->output, expected, fixture->output_length) == 0;
}

static bool output_contains(const Fixture *fixture, const char *needle) {
    return strstr(fixture->output, needle) != NULL;
}

static size_t output_occurrences(const Fixture *fixture, const char *needle) {
    size_t count = 0;
    size_t length = strlen(needle);
    const char *cursor = fixture->output;

    if (length == 0u) return 0;
    while ((cursor = strstr(cursor, needle)) != NULL) {
        ++count;
        cursor += length;
    }
    return count;
}

static void store_word(Fixture *fixture, uint32_t pa, uint32_t value) {
    fixture->memory[pa] = (uint8_t)value;
    fixture->memory[pa + 1u] = (uint8_t)(value >> 8);
    fixture->memory[pa + 2u] = (uint8_t)(value >> 16);
    fixture->memory[pa + 3u] = (uint8_t)(value >> 24);
}

static int test_pause_gate(void) {
    static const char *const commands[] = {
        "help",
        "tlb 0",
        "translate 0x80000000",
        "reg t0",
        "reg t0 1",
        "mrb 0",
        "mwb 0 1",
        "disasm 0",
    };
    Fixture fixture;
    Registers before;
    uint8_t memory_before[TEST_MEMORY_SIZE];

    CHECK(fixture_init(&fixture));
    fixture.halted = false;
    fixture.state.gpr[8] = UINT32_C(0x12345678);
    fixture.memory[0] = 0xa5u;
    before = fixture.state;
    memcpy(memory_before, fixture.memory, sizeof(memory_before));

    for (size_t index = 0;
         index < sizeof(commands) / sizeof(commands[0]); ++index) {
        clear_bus_log(&fixture);
        CHECK(execute(&fixture, commands[index]) ==
              MIPSEL_CONSOLE_TARGET_NOT_HALTED);
        CHECK(output_is(&fixture, "error: target not halted\r\n"));
        CHECK(fixture.read_calls == 0u && fixture.write_calls == 0u);
        CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);
        CHECK(memcmp(fixture.memory, memory_before,
                     sizeof(memory_before)) == 0);
    }

    CHECK(execute(&fixture, " \t\r\n") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture, ""));
    fixture.halted = true;
    CHECK(execute(&fixture, "help") == MIPSEL_CONSOLE_OK);
    CHECK(output_contains(&fixture, "tlb [index]"));
    return 0;
}

static int test_parser_and_numeric_boundaries(void) {
    static const char *const invalid_values[] = {
        "reg t0 -1",
        "reg t0 +1",
        "reg t0 4294967296",
        "reg t0 0x100000000",
        "reg t0 0x",
        "reg t0 12junk",
        "reg t0 1 2",
        "reg t0 1 two extra",
    };
    Fixture fixture;
    uint32_t old_value;
    char boundary[MIPSEL_EMU_CONSOLE_LINE_SIZE];
    const char embedded_nul[] = {'h', 'e', 'l', 'p', '\0', 'x'};

    CHECK(fixture_init(&fixture));
    CHECK(execute(&fixture, " \t ReG\t$T0\t0XFFFFFFFF\v") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[8] == UINT32_MAX);
    CHECK(output_is(&fixture, "$t0 <- 0xffffffff\r\n"));

    CHECK(execute(&fixture, "reg r8 08") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[8] == 8u);
    CHECK(execute(&fixture, "reg 8 4294967295") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[8] == UINT32_MAX);

    old_value = fixture.state.gpr[8];
    for (size_t index = 0;
         index < sizeof(invalid_values) / sizeof(invalid_values[0]); ++index) {
        CHECK(execute(&fixture, invalid_values[index]) ==
              MIPSEL_CONSOLE_ERROR);
        CHECK(fixture.state.gpr[8] == old_value);
        CHECK(fixture.output_length != 0u && !fixture.output_overflow);
    }

    CHECK(execute(&fixture, "unknown") == MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "unknown command"));
    clear_output(&fixture);
    CHECK(mipsel_console_execute(&fixture.console, embedded_nul,
                                 sizeof(embedded_nul)) ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "NUL byte"));

    memset(boundary, ' ', sizeof(boundary));
    clear_output(&fixture);
    CHECK(mipsel_console_execute(&fixture.console, boundary,
                                 sizeof(boundary) - 1u) ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture, ""));
    clear_output(&fixture);
    CHECK(mipsel_console_execute(&fixture.console, boundary,
                                 sizeof(boundary)) ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "too long"));

    clear_output(&fixture);
    CHECK(mipsel_console_execute(&fixture.console, NULL, 1u) ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(output_is(&fixture, ""));
    return 0;
}

static int test_register_commands(void) {
    static const char *const aliases[] = {"t0", "$T0", "r8", "8"};
    Fixture fixture;
    Registers before;

    CHECK(fixture_init(&fixture));
    fixture.state.gpr[8] = UINT32_C(0x12345678);
    for (size_t index = 0;
         index < sizeof(aliases) / sizeof(aliases[0]); ++index) {
        char command[32];
        (void)snprintf(command, sizeof(command), "reg %s", aliases[index]);
        CHECK(execute(&fixture, command) == MIPSEL_CONSOLE_OK);
        CHECK(output_is(&fixture, "$t0 = 0x12345678\r\n"));
    }

    CHECK(execute(&fixture, "reg t0 0x89abcdef") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[8] == UINT32_C(0x89abcdef));
    CHECK(output_is(&fixture, "$t0 <- 0x89abcdef\r\n"));

    fixture.state.gpr[0] = 0;
    CHECK(execute(&fixture, "reg zero 1") == MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.state.gpr[0] == 0u);
    CHECK(output_contains(&fixture, "read-only"));

    fixture.state.pc = UINT32_C(0x80001000);
    fixture.state.next_pc = UINT32_C(0x81234567);
    fixture.state.is_delay_slot = 1;
    fixture.state.is_taken = 1;
    fixture.state.target_pc = UINT32_C(0x87654321);
    fixture.state.bds = 1;
    fixture.state.exception_pending = 1;
    fixture.state.ll_bit = 1;
    fixture.state.ll_addr = UINT32_C(0x00123000);
    CHECK(execute(&fixture, "reg pc 0x80002000") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.pc == UINT32_C(0x80002000));
    CHECK(fixture.state.next_pc == UINT32_C(0x80002004));
    CHECK(!fixture.state.is_delay_slot && !fixture.state.is_taken);
    CHECK(fixture.state.target_pc == 0u && !fixture.state.bds);
    CHECK(!fixture.state.exception_pending);
    CHECK(!fixture.state.ll_bit && fixture.state.ll_addr == 0u);

    fixture.state.ll_bit = 1;
    fixture.state.ll_addr = UINT32_C(0x00456000);
    CHECK(execute(&fixture, "reg next_pc 0x80003000") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.next_pc == UINT32_C(0x80003000));
    CHECK(!fixture.state.ll_bit && fixture.state.ll_addr == 0u);
    CHECK(execute(&fixture, "reg hi 0x11112222") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(execute(&fixture, "reg lo 0x33334444") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.hi == UINT32_C(0x11112222));
    CHECK(fixture.state.lo == UINT32_C(0x33334444));

    CHECK(execute(&fixture, "reg status 0x00400001") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.cp0.regs[12][0] == UINT32_C(0x00400001));
    CHECK(execute(&fixture, "reg cp0.12.0") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture, "cp0.12.0 = 0x00400001\r\n"));

    fixture.state.cp0.byname.cp0r13_t.cp0r13_n.Cause =
        (UINT32_C(1) << CP0_CAUSE_TI_POS) |
        (UINT32_C(1) << (CP0_CAUSE_IP_POS + 7));
    CHECK(execute(&fixture, "reg compare 0x12345678") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.cp0.byname.cp0r11_t.cp0r11_n.Compare ==
          UINT32_C(0x12345678));
    CHECK(!GET_BITFIELD(fixture.state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                        CP0_CAUSE_TI_POS, CP0_CAUSE_TI_LEN));
    CHECK(!GET_BITFIELD(fixture.state.cp0.byname.cp0r13_t.cp0r13_n.Cause,
                        CP0_CAUSE_IP_POS + 7, 1));

    fixture.state.cp0.byname.cp0r13_t.cp0r13_n.Cause = UINT32_C(0x8000007c);
    CHECK(execute(&fixture, "reg cause 0xffffffff") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK((fixture.state.cp0.byname.cp0r13_t.cp0r13_n.Cause &
           UINT32_C(0x8000007c)) == UINT32_C(0x8000007c));
    CHECK((fixture.state.cp0.byname.cp0r13_t.cp0r13_n.Cause &
           (UINT32_C(3) << CP0_CAUSE_IP_POS)) ==
          (UINT32_C(3) << CP0_CAUSE_IP_POS));

    before = fixture.state;
    CHECK(execute(&fixture, "reg cp0.32.0") == MIPSEL_CONSOLE_ERROR);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);
    CHECK(execute(&fixture, "reg cp0.12.8") == MIPSEL_CONSOLE_ERROR);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);
    CHECK(execute(&fixture, "reg no_such_register") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);

    before = fixture.state;
    CHECK(execute(&fixture, "reg") == MIPSEL_CONSOLE_OK);
    CHECK(output_contains(&fixture, "$zero="));
    CHECK(output_contains(&fixture, "$ra="));
    CHECK(output_contains(&fixture, "pc=80002000"));
    CHECK(output_contains(&fixture, "status=00400001"));
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);
    CHECK(!fixture.output_overflow);
    return 0;
}

static int test_tlb_commands(void) {
    Fixture fixture;
    Registers before;

    CHECK(fixture_init(&fixture));
    fixture.state.tlb[5].entryhi = UINT32_C(0x0040002a);
    fixture.state.tlb[5].entrylo0 = UINT32_C(0x000048c7);
    fixture.state.tlb[5].entrylo1 = UINT32_C(0x00011587);
    fixture.state.tlb[5].pmask = 0;
    fixture.state.tlb[63].entryhi = UINT32_C(0xdeadbeef);
    fixture.state.tlb[63].entrylo0 = UINT32_C(0x11111111);
    fixture.state.tlb[63].entrylo1 = UINT32_C(0x22222222);
    fixture.state.tlb[63].pmask = UINT32_C(0x00006000);

    before = fixture.state;
    CHECK(execute(&fixture, "tlb 5") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "tlb[05] hi=0040002a lo0=000048c7 "
                    "lo1=00011587 mask=00000000\r\n"));
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);
    CHECK(execute(&fixture, "tlb 0x3f") == MIPSEL_CONSOLE_OK);
    CHECK(output_contains(&fixture, "tlb[63] hi=deadbeef"));

    CHECK(execute(&fixture, "tlb 64") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "tlb -1") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "tlb 4294967296") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "tlb 1 2") == MIPSEL_CONSOLE_ERROR);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);

    CHECK(execute(&fixture, "tlb") == MIPSEL_CONSOLE_OK);
    CHECK(output_occurrences(&fixture, "tlb[") == 64u);
    CHECK(output_contains(&fixture, "tlb[00]"));
    CHECK(output_contains(&fixture, "tlb[63]"));
    CHECK(!fixture.output_overflow);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);
    return 0;
}

static int test_translate_commands(void) {
    Fixture fixture;
    Registers before;

    CHECK(fixture_init(&fixture));
    fixture.state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    CHECK(execute(&fixture, "translate 0x81234567") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0x81234567 -> PA 0x01234567\r\n"));
    CHECK(execute(&fixture, "translate 0xa1234567") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0xa1234567 -> PA 0x01234567\r\n"));

    fixture.state.cp0.byname.cp0r12_t.cp0r12_n.Status =
        UINT32_C(1) << CP0_STATUS_ERL_POS;
    CHECK(execute(&fixture, "translate 0x00123456") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0x00123456 -> PA 0x00123456\r\n"));

    fixture.state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    fixture.state.cp0.byname.cp0r10_t.cp0r10_n.EntryHi = 0x2au;
    memset(fixture.state.tlb, 0, sizeof(fixture.state.tlb));
    fixture.state.tlb[5].entryhi = UINT32_C(0x0040002a);
    fixture.state.tlb[5].entrylo0 = UINT32_C(0x000048c7);
    fixture.state.tlb[5].entrylo1 = UINT32_C(0x00011587);
    before = fixture.state;
    CHECK(execute(&fixture, "translate 0x00400123") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0x00400123 -> PA 0x00123123\r\n"));
    CHECK(execute(&fixture, "translate 0x00401123") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0x00401123 -> PA 0x00456123\r\n"));
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);

    memset(fixture.state.tlb, 0, sizeof(fixture.state.tlb));
    fixture.state.tlb[7].entryhi = UINT32_C(0x0080002a);
    fixture.state.tlb[7].entrylo0 = UINT32_C(0x00004006);
    fixture.state.tlb[7].entrylo1 = UINT32_C(0x00008006);
    fixture.state.tlb[7].pmask = UINT32_C(0x00006000);
    CHECK(execute(&fixture, "translate 0x00804567") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0x00804567 -> PA 0x00200567\r\n"));

    fixture.state.tlb[7].entryhi = UINT32_C(0x0080002b);
    CHECK(execute(&fixture, "translate 0x00804567") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(output_is(&fixture, "VA 0x00804567: TLB refill\r\n"));
    fixture.state.tlb[7].entrylo0 |= 1u;
    fixture.state.tlb[7].entrylo1 |= 1u;
    CHECK(execute(&fixture, "translate 0x00804567") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "VA 0x00804567 -> PA 0x00200567\r\n"));

    memset(fixture.state.tlb, 0, sizeof(fixture.state.tlb));
    fixture.state.tlb[0].entryhi = UINT32_C(0x0040002a);
    fixture.state.tlb[0].entrylo0 = UINT32_C(0x00004804);
    fixture.state.tlb[0].entrylo1 = UINT32_C(0x00011584);
    CHECK(execute(&fixture, "translate 0x00400123") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(output_is(&fixture, "VA 0x00400123: TLB invalid\r\n"));

    CHECK(execute(&fixture, "translate") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "translate 0x100000000") ==
          MIPSEL_CONSOLE_ERROR);
    return 0;
}

static int test_memory_commands(void) {
    Fixture fixture;
    Registers before;
    uint8_t memory_before[TEST_MEMORY_SIZE];
    unsigned calls;

    CHECK(fixture_init(&fixture));
    fixture.memory[4] = 0x12u;
    fixture.memory[5] = 0x34u;
    fixture.memory[6] = 0x56u;
    fixture.memory[7] = 0x78u;
    before = fixture.state;
    before.ll_bit = 0;
    before.ll_addr = 0;
    fixture.state.ll_bit = 1;
    fixture.state.ll_addr = 4u;

    clear_bus_log(&fixture);
    CHECK(execute(&fixture, "mrb 4") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture, "mrb 0x00000004 = 0x12\r\n"));
    CHECK(fixture.read_calls == 1u && fixture.last_pa == 4u &&
          fixture.last_width == 1u);
    CHECK(execute(&fixture, "mrh 4") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture, "mrh 0x00000004 = 0x3412\r\n"));
    CHECK(fixture.last_pa == 4u && fixture.last_width == 2u);
    CHECK(execute(&fixture, "mrw 4") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "mrw 0x00000004 = 0x78563412\r\n"));
    CHECK(fixture.last_pa == 4u && fixture.last_width == 4u);

    CHECK(execute(&fixture, "mwb 9 0xab") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.memory[9] == 0xabu && fixture.last_width == 1u);
    CHECK(output_is(&fixture, "mwb 0x00000009 <- 0xab\r\n"));
    CHECK(execute(&fixture, "mwh 10 0xcdef") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.memory[10] == 0xefu && fixture.memory[11] == 0xcdu);
    CHECK(fixture.last_width == 2u);
    CHECK(execute(&fixture, "mww 13 0x89abcdef") ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.memory[13] == 0xefu && fixture.memory[14] == 0xcdu &&
          fixture.memory[15] == 0xabu && fixture.memory[16] == 0x89u);
    CHECK(fixture.last_pa == 13u && fixture.last_width == 4u);
    CHECK(!fixture.state.ll_bit && fixture.state.ll_addr == 0u);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);

    calls = fixture.write_calls;
    CHECK(execute(&fixture, "mwb 0 256") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "mwh 0 65536") == MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.write_calls == calls);
    CHECK(execute(&fixture, "mww 0 4294967296") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.write_calls == calls);

    calls = fixture.read_calls;
    CHECK(execute(&fixture, "mrb 0x100000000") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.read_calls == calls);
    CHECK(execute(&fixture, "mrw 510") == MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.read_calls == calls + 1u);
    CHECK(fixture.last_pa == 510u && fixture.last_width == 4u);

    calls = fixture.read_calls;
    CHECK(execute(&fixture, "mrb 0x80000010") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.read_calls == calls + 1u);
    CHECK(fixture.last_pa == UINT32_C(0x80000010));

    fixture.fail_read = true;
    calls = fixture.read_calls;
    CHECK(execute(&fixture, "mrb 0") == MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.read_calls == calls + 1u);
    CHECK(output_contains(&fixture, "cannot read PA"));
    fixture.fail_read = false;

    memcpy(memory_before, fixture.memory, sizeof(memory_before));
    fixture.fail_write = true;
    calls = fixture.write_calls;
    CHECK(execute(&fixture, "mww 20 0x11223344") ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.write_calls == calls + 1u);
    CHECK(memcmp(fixture.memory, memory_before,
                 sizeof(memory_before)) == 0);
    fixture.fail_write = false;

    CHECK(execute(&fixture, "mrb") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "mrb 0 extra") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "mww 0") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "mww 0 1 extra") == MIPSEL_CONSOLE_ERROR);

    fixture.console.config.bus_read = NULL;
    CHECK(execute(&fixture, "mrb 0") == MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "read is unavailable"));
    fixture.console.config.bus_read = bus_read;
    fixture.console.config.bus_write = NULL;
    CHECK(execute(&fixture, "mwb 0 1") == MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "write is unavailable"));
    return 0;
}

static int test_disasm_commands(void) {
    Fixture fixture;
    Registers before;
    unsigned calls;

    CHECK(fixture_init(&fixture));
    fixture.state.pc = UINT32_C(0x00001000);
    fixture.state.next_pc = UINT32_C(0x00001004);
    before = fixture.state;
    clear_bus_log(&fixture);
    CHECK(execute(&fixture, "disasm 0x1085fffe") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "0x00001000: 0x1085fffe  "
                    "beq $a0, $a1, 0x00000ffc\r\n"));
    CHECK(fixture.read_calls == 0u);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);

    fixture.state.pc = UINT32_C(0x0ffffffc);
    fixture.state.next_pc = UINT32_C(0x10000000);
    CHECK(execute(&fixture, "disasm 0x08000001") ==
          MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "0x0ffffffc: 0x08000001  j 0x10000004\r\n"));

    fixture.state.pc = UINT32_C(0x80000100);
    fixture.state.next_pc = UINT32_C(0x80000104);
    store_word(&fixture, 0x100u, UINT32_C(0x2482ffff));
    clear_bus_log(&fixture);
    before = fixture.state;
    CHECK(execute(&fixture, "disasm") == MIPSEL_CONSOLE_OK);
    CHECK(output_is(&fixture,
                    "0x80000100: 0x2482ffff  "
                    "addiu $v0, $a0, -1\r\n"));
    CHECK(fixture.read_calls == 1u && fixture.last_pa == 0x100u &&
          fixture.last_width == 4u);
    CHECK(memcmp(&fixture.state, &before, sizeof(before)) == 0);

    fixture.fail_read = true;
    calls = fixture.read_calls;
    CHECK(execute(&fixture, "disasm") == MIPSEL_CONSOLE_ERROR);
    CHECK(fixture.read_calls == calls + 1u);
    CHECK(output_contains(&fixture, "cannot read instruction"));
    fixture.fail_read = false;

    fixture.state.pc = UINT32_C(0x80000101);
    clear_bus_log(&fixture);
    CHECK(execute(&fixture, "disasm") == MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "not word-aligned"));
    CHECK(fixture.read_calls == 0u);

    fixture.state.pc = UINT32_C(0x00400000);
    fixture.state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    fixture.state.cp0.byname.cp0r10_t.cp0r10_n.EntryHi = 0x2au;
    memset(fixture.state.tlb, 0, sizeof(fixture.state.tlb));
    CHECK(execute(&fixture, "disasm") == MIPSEL_CONSOLE_ERROR);
    CHECK(output_is(&fixture, "PC 0x00400000: TLB refill\r\n"));

    CHECK(execute(&fixture, "disasm xyz") == MIPSEL_CONSOLE_ERROR);
    CHECK(execute(&fixture, "disasm 0 1") == MIPSEL_CONSOLE_ERROR);
    return 0;
}

static int test_cdc_feed(void) {
    static const uint8_t fragmented[] = "reg t0 42\r";
    static const uint8_t multiple[] =
        "reg t1 1\r\nreg t2 2\n";
    static const uint8_t changed_then_blank[] =
        "reg t1 3\n\n";
    static const uint8_t error_then_changed[] =
        "no-such-command\nreg t1 4\n";
    static const uint8_t backspace[] = {
        'r', 'e', 'g', ' ', 't', '3', ' ', '1', 'x', 0x08u, '2', '\n'
    };
    static const uint8_t delete_key[] = {
        'r', 'e', 'g', ' ', 't', '4', ' ', '9', 'x', 0x7fu, '8', '\n'
    };
    static const uint8_t clear_line = 0x15u;
    Fixture fixture;
    uint8_t too_long[MIPSEL_EMU_CONSOLE_LINE_SIZE];
    mipsel_console_result_t result = MIPSEL_CONSOLE_NO_COMMAND;

    CHECK(fixture_init(&fixture));
    clear_output(&fixture);
    for (size_t index = 0; index < sizeof(fragmented) - 1u; ++index) {
        result = mipsel_console_feed(&fixture.console,
                                     &fragmented[index], 1u);
        if (index + 1u < sizeof(fragmented) - 1u)
            CHECK(result == MIPSEL_CONSOLE_NO_COMMAND);
    }
    CHECK(result == MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[8] == 42u);
    CHECK(output_is(&fixture, "$t0 <- 0x0000002a\r\n"));
    CHECK(mipsel_console_input_length(&fixture.console) == 0u);

    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console,
                              (const uint8_t *)"\n", 1u) ==
          MIPSEL_CONSOLE_NO_COMMAND);
    CHECK(output_is(&fixture, ""));

    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console, multiple,
                              sizeof(multiple) - 1u) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[9] == 1u && fixture.state.gpr[10] == 2u);
    CHECK(output_occurrences(&fixture, " <- ") == 2u);

    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console, changed_then_blank,
                              sizeof(changed_then_blank) - 1u) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[9] == 3u);
    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console, error_then_changed,
                              sizeof(error_then_changed) - 1u) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[9] == 4u);
    CHECK(output_contains(&fixture, "unknown command"));

    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console, backspace,
                              sizeof(backspace)) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[11] == 12u);
    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console, delete_key,
                              sizeof(delete_key)) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[12] == 98u);

    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console,
                              (const uint8_t *)"reg t5 111", 10u) ==
          MIPSEL_CONSOLE_NO_COMMAND);
    CHECK(mipsel_console_input_length(&fixture.console) == 10u);
    CHECK(mipsel_console_feed(&fixture.console, &clear_line, 1u) ==
          MIPSEL_CONSOLE_NO_COMMAND);
    CHECK(mipsel_console_input_length(&fixture.console) == 0u);
    CHECK(strcmp(mipsel_console_input(&fixture.console), "") == 0);
    CHECK(mipsel_console_feed(&fixture.console,
                              (const uint8_t *)"reg t5 7\n", 9u) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[13] == 7u);

    memset(too_long, 'x', sizeof(too_long));
    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console, too_long,
                              sizeof(too_long)) ==
          MIPSEL_CONSOLE_NO_COMMAND);
    CHECK(mipsel_console_input_length(&fixture.console) ==
          MIPSEL_EMU_CONSOLE_LINE_SIZE - 1u);
    CHECK(mipsel_console_feed(&fixture.console,
                              (const uint8_t *)"\n", 1u) ==
          MIPSEL_CONSOLE_ERROR);
    CHECK(output_contains(&fixture, "command line is too long"));
    CHECK(mipsel_console_input_length(&fixture.console) == 0u);
    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console,
                              (const uint8_t *)"reg t6 77\n", 10u) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[14] == 77u);

    clear_output(&fixture);
    CHECK(mipsel_console_feed(&fixture.console,
                              (const uint8_t *)"reg t7 6\r\n", 10u) ==
          MIPSEL_CONSOLE_STATE_CHANGED);
    CHECK(fixture.state.gpr[15] == 6u);
    CHECK(output_occurrences(&fixture, "$t7 <-") == 1u);
    CHECK(!fixture.output_overflow);
    return 0;
}

typedef int (*test_fn)(void);

typedef struct {
    const char *name;
    test_fn function;
} TestCase;

int main(void) {
    static const TestCase tests[] = {
        {"pause gate", test_pause_gate},
        {"parser and numeric boundaries", test_parser_and_numeric_boundaries},
        {"register commands", test_register_commands},
        {"TLB commands", test_tlb_commands},
        {"translation", test_translate_commands},
        {"physical memory", test_memory_commands},
        {"disassembly", test_disasm_commands},
        {"CDC feed", test_cdc_feed},
    };

    for (size_t index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        int result = tests[index].function();
        if (result != 0) {
            fprintf(stderr, "console test failed: %s (line %d)\n",
                    tests[index].name, result);
            return (int)index + 1;
        }
    }
    return 0;
}
