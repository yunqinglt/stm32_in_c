#define _POSIX_C_SOURCE 200809L

#include "emu.h"
#include "exception.h"
#include "platform.h"
#include "registers.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint64_t monotonic_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}

int main(int argc, char **argv)
{
    uint32_t ticks = UINT32_C(100000000);
    uint8_t *ram;
    Registers state = {0};
    uint64_t started;
    uint64_t elapsed;

    if (argc == 2) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[1], &end, 10);
        if (!end || *end != '\0' || parsed == 0 || parsed > UINT32_MAX)
            return 2;
        ticks = (uint32_t)parsed;
    }

    ram = calloc(1, 4096u);
    if (!ram || !platform_memory_bind(ram, 4096u))
        return 3;
    platform_init(NULL, NULL);
    reset_cpu(&state);
    state.cp0.byname.cp0r12_t.cp0r12_n.Status = 0;
    state.pc = UINT32_C(0x80000000);
    state.next_pc = state.pc + 4u;

    /* addiu; addu; unconditional branch; architectural delay-slot nop. */
    write32(0u, UINT32_C(0x25080001));
    write32(4u, UINT32_C(0x01084821));
    write32(8u, UINT32_C(0x1000fffd));
    write32(12u, 0u);

    /* Prime demand paging/caches and CPU frequency before timed execution. */
    mipsel_emu_run_steps(&state, UINT32_C(1000000));
    started = monotonic_ns();
    mipsel_emu_run_steps(&state, ticks);
    elapsed = monotonic_ns() - started;

    if (elapsed == 0)
        return 4;
    printf("ticks=%" PRIu32 " elapsed_ns=%" PRIu64
           " ticks_per_second=%.2f registers_size=%zu final_t0=%" PRIu32 "\n",
           ticks, elapsed, (double)ticks * 1000000000.0 / (double)elapsed,
           sizeof(state), state.gpr[8]);
    free(ram);
    return 0;
}
