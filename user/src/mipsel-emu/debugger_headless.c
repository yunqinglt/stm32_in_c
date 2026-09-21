#include "config.h"
#include "debugger.h"
#include "observer.h"
#include "platform.h"

#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Hosted frontends that do not have a POSIX terminal still need the debugger
 * ABI used by runloop.c and loader.c.  This backend deliberately keeps no
 * terminal state: it records the optional trace, forwards UART bytes to the
 * process stdout, and lets the frontend request shutdown explicitly.
 */
typedef struct {
    bool initialized;
    bool quit;
    FILE *trace_file;
    bool trace_file_owned;
#ifdef _WIN32
    void (*previous_sigint)(int);
    bool sigint_installed;
#endif
} HeadlessDebugger;

static HeadlessDebugger debugger;

#ifdef _WIN32
static volatile sig_atomic_t interrupt_requested;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    interrupt_requested = 1;
}
#endif

static void trace_line(const char *format, ...)
{
    va_list arguments;

    if (!debugger.trace_file) return;
    va_start(arguments, format);
    vfprintf(debugger.trace_file, format, arguments);
    va_end(arguments);
    fputc('\n', debugger.trace_file);
}

static void observer_instruction_begin(void *opaque, uint32_t pc,
                                       uint32_t pa, uint32_t word,
                                       const Registers *state)
{
    (void)opaque;
    (void)state;
    trace_line("I pc=%08" PRIx32 " pa=%08" PRIx32 " word=%08" PRIx32,
               pc, pa, word);
}

static void observer_instruction_end(void *opaque, const Registers *state)
{
    (void)opaque;
    (void)state;
}

static void observer_exception(void *opaque, const Registers *state,
                               uint32_t exc_info, uint8_t exc_code,
                               VectorClass vector_class)
{
    (void)opaque;
    (void)state;
    trace_line("X code=%u info=%08" PRIx32 " vector=%u",
               (unsigned)exc_code, exc_info, (unsigned)vector_class);
}

static const mipsel_emu_observer_t observer = {
    NULL,
    observer_instruction_begin,
    observer_instruction_end,
    observer_exception,
};

int debugger_init(const DebuggerConfig *config, Registers *state,
                  vmstate_t *vm)
{
    const char *path;

    (void)state;
    if (!config || !vm) return -1;
    debugger_shutdown();
    debugger = (HeadlessDebugger){0};
    debugger.initialized = true;
#ifdef _WIN32
    interrupt_requested = 0;
    debugger.previous_sigint = signal(SIGINT, handle_sigint);
    if (debugger.previous_sigint != SIG_ERR)
        debugger.sigint_installed = true;
#endif
    path = config->trace_path;
    if (path && path[0] != '\0') {
        if (strcmp(path, "-") == 0) {
            debugger.trace_file = stderr;
        } else {
            debugger.trace_file = fopen(path, "wb");
            if (!debugger.trace_file) {
                debugger_shutdown();
                return -1;
            }
            debugger.trace_file_owned = true;
        }
    }
    debugger.quit = false;
    mipsel_emu_observer_set(&observer);
    if (config->start_paused) {
        vm->state = STEPPING;
        vm->steps = 0;
    }
    return 0;
}

void debugger_shutdown(void)
{
    if (!debugger.initialized && !debugger.trace_file
#ifdef _WIN32
        && !debugger.sigint_installed
#endif
    ) return;
    mipsel_emu_observer_set(NULL);
#ifdef _WIN32
    if (debugger.sigint_installed)
        (void)signal(SIGINT, debugger.previous_sigint);
#endif
    if (debugger.trace_file_owned && debugger.trace_file)
        fclose(debugger.trace_file);
    debugger = (HeadlessDebugger){0};
}

void debugger_instruction_begin(uint32_t pc, uint32_t pa, uint32_t word,
                                const Registers *state)
{
    observer_instruction_begin(NULL, pc, pa, word, state);
}

void debugger_instruction_end(const Registers *state)
{
    observer_instruction_end(NULL, state);
}

void debugger_exception(const Registers *state, uint32_t exc_info,
                        uint8_t exc_code, VectorClass vector_class)
{
    observer_exception(NULL, state, exc_info, exc_code, vector_class);
}

void debugger_uart_tx(uint8_t byte)
{
    if (fwrite(&byte, 1, 1, stdout) == 1) fflush(stdout);
}

void debugger_board_reset(const Registers *state)
{
    (void)state;
    trace_line("RESET");
}

void debugger_poll(Registers *state, vmstate_t *vm, bool wait_for_input)
{
    (void)state;
    (void)vm;
    (void)wait_for_input;
    /* There is no terminal input path in the Windows headless backend. */
}

bool debugger_quit_requested(void)
{
#ifdef _WIN32
    if (interrupt_requested) debugger.quit = true;
#endif
    return debugger.quit;
}

void debugger_request_quit(void)
{
    debugger.quit = true;
}

bool debugger_tui_enabled(void)
{
    return false;
}
