#include "config.h"
#include "emu.h"
#include "exception.h"
#include "platform.h"
#include "registers.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#define MIPSEL_EMU_BENCHMARK_TIMER "windows-qpc"
#else
#define MIPSEL_EMU_BENCHMARK_TIMER "c11-timespec-get"
#endif

#ifndef MIPSEL_EMU_BENCHMARK_DISPATCH_MODE
#define MIPSEL_EMU_BENCHMARK_DISPATCH_MODE "unknown"
#endif

#ifndef MIPSEL_EMU_BENCHMARK_COMPILER_ID
#define MIPSEL_EMU_BENCHMARK_COMPILER_ID "unknown"
#endif

#ifndef MIPSEL_EMU_BENCHMARK_COMPILER_VERSION
#define MIPSEL_EMU_BENCHMARK_COMPILER_VERSION "unknown"
#endif

#ifndef MIPSEL_EMU_BENCHMARK_BUILD_CONFIG
#define MIPSEL_EMU_BENCHMARK_BUILD_CONFIG "unknown"
#endif

#define DEFAULT_STEPS UINT32_C(100000000)
#define DEFAULT_ROUNDS UINT32_C(5)
#define DEFAULT_WARMUP_STEPS UINT32_C(1000000)
#define MAX_ROUNDS UINT32_C(1000)

static int wall_time_ns(uint64_t *value)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    static LONGLONG frequency;
    uint64_t seconds;
    uint64_t remainder;

    if (!value)
        return 0;
    if (frequency == 0) {
        LARGE_INTEGER detected_frequency;

        if (!QueryPerformanceFrequency(&detected_frequency) ||
            detected_frequency.QuadPart <= 0)
            return 0;
        frequency = detected_frequency.QuadPart;
    }
    if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0)
        return 0;
    seconds = (uint64_t)(counter.QuadPart / frequency);
    remainder = (uint64_t)(counter.QuadPart % frequency);
    *value = seconds * UINT64_C(1000000000) +
             remainder * UINT64_C(1000000000) /
                 (uint64_t)frequency;
    return 1;
#else
    struct timespec now;

    if (!value || timespec_get(&now, TIME_UTC) != TIME_UTC)
        return 0;
    *value = (uint64_t)now.tv_sec * UINT64_C(1000000000) +
             (uint64_t)now.tv_nsec;
    return 1;
#endif
}

static int parse_u32(const char *text, uint32_t minimum, uint32_t maximum,
                     uint32_t *value)
{
    char *end = NULL;
    const char *cursor;
    unsigned long long parsed;

    if (!text || !value || text[0] == '\0')
        return 0;
    for (cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9')
            return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || !end || *end != '\0' || parsed < minimum ||
        parsed > maximum)
        return 0;
    *value = (uint32_t)parsed;
    return 1;
}

static int compare_u64(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;

    return (a > b) - (a < b);
}

static double rate_msteps(uint32_t steps, uint64_t elapsed_ns)
{
    return elapsed_ns == 0
               ? 0.0
               : (double)steps * 1000.0 / (double)elapsed_ns;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [steps-per-round [rounds [warmup-steps]]]\n"
            "  defaults: steps-per-round=%" PRIu32
            " rounds=%" PRIu32 " warmup-steps=%" PRIu32 "\n",
            program, DEFAULT_STEPS, DEFAULT_ROUNDS, DEFAULT_WARMUP_STEPS);
}

static void reset_workload(Registers *state)
{
    reset_cpu(state);
    state->cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state->pc = UINT32_C(0x80000000);
    state->next_pc = state->pc + 4u;
}

static int validate_workload(const Registers *state, uint32_t steps)
{
    static const uint32_t expected_pc[4] = {
        UINT32_C(0x80000000), UINT32_C(0x80000004),
        UINT32_C(0x80000008), UINT32_C(0x8000000c),
    };
    static const uint32_t expected_next_pc[4] = {
        UINT32_C(0x80000004), UINT32_C(0x80000008),
        UINT32_C(0x8000000c), UINT32_C(0x80000000),
    };
    uint32_t complete_loops = steps / 4u;
    uint32_t remainder = steps % 4u;
    uint32_t expected_t0 = complete_loops + (remainder >= 1u ? 1u : 0u);
    uint32_t expected_t1 =
        (complete_loops + (remainder >= 2u ? 1u : 0u)) * 2u;

    return state && state->gpr[0] == 0u && state->gpr[8] == expected_t0 &&
           state->gpr[9] == expected_t1 &&
           state->pc == expected_pc[remainder] &&
           state->next_pc == expected_next_pc[remainder] &&
           state->bds == (remainder == 3u) &&
           state->exception_pending == 0u &&
           state->cp0.byname.cp0r9_t.cp0r9_n.Count ==
               steps / MIPSEL_EMU_CP0_COUNT_DIVIDER &&
           state->cp0_count_divider ==
               steps % MIPSEL_EMU_CP0_COUNT_DIVIDER;
}

static void report_validation_failure(const Registers *state, uint32_t steps)
{
    fprintf(stderr,
            "workload validation failed after %" PRIu32
            " steps: pc=0x%08" PRIx32 " next_pc=0x%08" PRIx32
            " t0=%" PRIu32 " t1=%" PRIu32 " count=%" PRIu32
            " divider=%u bds=%u exception_pending=%u\n",
            steps, state->pc, state->next_pc, state->gpr[8], state->gpr[9],
            state->cp0.byname.cp0r9_t.cp0r9_n.Count,
            (unsigned)state->cp0_count_divider, (unsigned)state->bds,
            (unsigned)state->exception_pending);
}

int main(int argc, char **argv)
{
    uint32_t steps = DEFAULT_STEPS;
    uint32_t rounds = DEFAULT_ROUNDS;
    uint32_t warmup_steps = DEFAULT_WARMUP_STEPS;
    uint8_t *ram = NULL;
    uint64_t *elapsed_ns = NULL;
    Registers state = {0};
    uint64_t total_elapsed_ns = 0;
    uint64_t median_elapsed_ns;
    uint64_t started;
    uint64_t stopped;
    uint32_t round;
    int result = 0;

    if (argc > 4 ||
        (argc >= 2 && !parse_u32(argv[1], 1u, UINT32_MAX, &steps)) ||
        (argc >= 3 && !parse_u32(argv[2], 1u, MAX_ROUNDS, &rounds)) ||
        (argc >= 4 &&
         !parse_u32(argv[3], 0u, UINT32_MAX, &warmup_steps))) {
        print_usage(argv[0]);
        return 2;
    }

    ram = (uint8_t *)calloc(1u, 4096u);
    elapsed_ns = (uint64_t *)calloc(rounds, sizeof(*elapsed_ns));
    if (!ram || !elapsed_ns || !platform_memory_bind(ram, 4096u)) {
        result = 3;
        goto cleanup;
    }

    platform_init(NULL, NULL);

    /* addiu; addu; unconditional branch; architectural delay-slot nop. */
    write32(0u, UINT32_C(0x25080001));
    write32(4u, UINT32_C(0x01084821));
    write32(8u, UINT32_C(0x1000fffd));
    write32(12u, 0u);

    reset_workload(&state);
    if (mipsel_emu_run_steps(&state, warmup_steps) != warmup_steps ||
        !validate_workload(&state, warmup_steps)) {
        report_validation_failure(&state, warmup_steps);
        result = 4;
        goto cleanup;
    }

    printf("dispatch=%s compiler=%s-%s config=%s timer=%s\n",
           MIPSEL_EMU_BENCHMARK_DISPATCH_MODE,
           MIPSEL_EMU_BENCHMARK_COMPILER_ID,
           MIPSEL_EMU_BENCHMARK_COMPILER_VERSION,
           MIPSEL_EMU_BENCHMARK_BUILD_CONFIG,
           MIPSEL_EMU_BENCHMARK_TIMER);
    printf("workload=alu-dependency-branch-delay-loop-v1"
           " warmup_steps=%" PRIu32 " steps_per_round=%" PRIu32
           " retired_instructions_per_round=%" PRIu32
           " rounds=%" PRIu32 "\n",
           warmup_steps, steps, steps, rounds);

    for (round = 0; round < rounds; ++round) {
        reset_workload(&state);
        if (!wall_time_ns(&started) ||
            mipsel_emu_run_steps(&state, steps) != steps ||
            !wall_time_ns(&stopped) || stopped <= started) {
            result = 5;
            goto cleanup;
        }
        if (!validate_workload(&state, steps)) {
            report_validation_failure(&state, steps);
            result = 6;
            goto cleanup;
        }

        elapsed_ns[round] = stopped - started;
        total_elapsed_ns += elapsed_ns[round];
        printf("round=%" PRIu32 " elapsed_ns=%" PRIu64
               " wall_seconds=%.9f msteps_per_second=%.3f\n",
               round + 1u, elapsed_ns[round],
               (double)elapsed_ns[round] / 1000000000.0,
               rate_msteps(steps, elapsed_ns[round]));
    }

    qsort(elapsed_ns, rounds, sizeof(*elapsed_ns), compare_u64);
    if ((rounds & 1u) != 0u) {
        median_elapsed_ns = elapsed_ns[rounds / 2u];
    } else {
        median_elapsed_ns =
            elapsed_ns[rounds / 2u - 1u] / 2u +
            elapsed_ns[rounds / 2u] / 2u +
            (elapsed_ns[rounds / 2u - 1u] & elapsed_ns[rounds / 2u] & 1u);
    }

    printf("summary total_steps=%" PRIu64
           " total_retired_instructions=%" PRIu64
           " total_elapsed_ns=%" PRIu64
           " median_msteps_per_second=%.3f"
           " min_msteps_per_second=%.3f max_msteps_per_second=%.3f"
           " final_pc=0x%08" PRIx32 " final_t0=%" PRIu32 "\n",
           (uint64_t)steps * rounds, (uint64_t)steps * rounds,
           total_elapsed_ns,
           rate_msteps(steps, median_elapsed_ns),
           rate_msteps(steps, elapsed_ns[rounds - 1u]),
           rate_msteps(steps, elapsed_ns[0]), state.pc, state.gpr[8]);

cleanup:
    free(elapsed_ns);
    free(ram);
    return result;
}
