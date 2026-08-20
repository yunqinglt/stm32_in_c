#include "debugger.h"
#include "emu.h"
#include "exception.h"
#include "platform.h"
#include "registers.h"
#include "runloop.h"

#include <stdbool.h>

/* Owned by the hosted loader frontend.  Embedded callers use cpu_step() or
 * mipsel_emu_run_steps() and therefore do not need this global. */
extern vmstate_t *status;

int startup(Registers *state) {
    while (!debugger_quit_requested()) {
        bool paused = status->state == STEPPING && status->steps == 0;
        debugger_poll(state, status, paused);
        if (debugger_quit_requested()) break;

        if (status->state == RESET) {
            if (status->reset_callback) {
                if (status->reset_callback(state, status->reset_opaque) != 0)
                    return -1;
            } else {
                reset_cpu(state);
            }
            platform_reset();
            debugger_board_reset(state);
            status->state = debugger_tui_enabled() ? STEPPING : RUNNING;
            status->steps = 0;
        } else if (status->state == RUNNING) {
            unsigned int batch = debugger_tui_enabled() ? 4096u : 65536u;
            while (batch-- && status->state == RUNNING &&
                   !debugger_quit_requested()) {
                cpu_step(state);
                update_cycle(state);
                ++status->ticks;
                if (status->max_ticks && status->ticks >= status->max_ticks)
                    return 0;
            }
        } else if (status->state == STEPPING && status->steps != 0) {
            cpu_step(state);
            update_cycle(state);
            ++status->ticks;
            status->steps -= 1;
            if (status->max_ticks && status->ticks >= status->max_ticks)
                return 0;
        }
    }
    return 0;
}
