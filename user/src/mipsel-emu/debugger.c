#include "config.h"
#include "debugger.h"
#if MIPSEL_EMU_ENABLE_CONSOLE
#include "console.h"
#endif
#include "disasm.h"
#include "observer.h"
#include "platform.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifdef MIPSEL_EMU_HAVE_CURSES
#include <curses.h>
#endif

#define TRACE_CAPACITY       512u
#define EXCEPTION_CAPACITY   128u
#define UART_LINE_CAPACITY   256u
#define MONITOR_LINE_CAPACITY 256u
#define DEBUG_LINE_LENGTH    192u
#define UART_LINE_LENGTH     160u
#define UART_INPUT_INITIAL_CAPACITY 1024u
#define UART_INPUT_MAX_CAPACITY     65536u
#define HANDLED_SIGNAL_COUNT        3u

typedef struct {
    uint32_t gpr[32];
    uint32_t hi;
    uint32_t lo;
    uint32_t status;
    uint32_t cause;
    uint32_t epc;
    uint32_t badvaddr;
} RegisterSnapshot;

typedef struct {
    bool initialized;
    bool capture_events;
    bool tui;
    bool in_instruction;
    bool uart_focus;
#if MIPSEL_EMU_ENABLE_CONSOLE
    bool monitor_focus;
#endif
    bool force_redraw;
    bool quit;
    bool stdin_closed;
    bool stdin_termios_saved;
    bool headless_escape;
    unsigned int refresh_hz;
    unsigned int active_page;
    unsigned int trace_scroll;
#if MIPSEL_EMU_ENABLE_CONSOLE
    unsigned int monitor_scroll;
#endif
    uint64_t sequence;
    uint64_t last_draw_ns;
    uint32_t instruction_pc;
    uint32_t instruction_pa;
    uint32_t instruction_word;
    uint32_t changed_gpr;
    RegisterSnapshot before;
    FILE *trace_file;
    bool trace_file_owned;
    size_t signal_action_count;
    struct sigaction previous_signal_actions[HANDLED_SIGNAL_COUNT];
    bool pending_instruction_exception;
    char instruction_exception[DEBUG_LINE_LENGTH];
    struct termios stdin_termios;

#ifdef MIPSEL_EMU_HAVE_CURSES
    SCREEN *curses_screen;
#endif

    char trace[TRACE_CAPACITY][DEBUG_LINE_LENGTH];
    size_t trace_head;
    size_t trace_count;
    char exceptions[EXCEPTION_CAPACITY][DEBUG_LINE_LENGTH];
    size_t exception_head;
    size_t exception_count;
    char uart_lines[UART_LINE_CAPACITY][UART_LINE_LENGTH];
    size_t uart_head;
    size_t uart_count;
    char uart_current[UART_LINE_LENGTH];
    size_t uart_current_length;
    uint8_t *uart_input;
    size_t uart_input_capacity;
    size_t uart_input_head;
    size_t uart_input_count;
#if MIPSEL_EMU_ENABLE_CONSOLE
    mipsel_console_t monitor;
    char monitor_lines[MONITOR_LINE_CAPACITY][DEBUG_LINE_LENGTH];
    size_t monitor_head;
    size_t monitor_count;
    char monitor_current[DEBUG_LINE_LENGTH];
    size_t monitor_current_length;
#endif
} DebuggerState;

static DebuggerState debugger;
static volatile sig_atomic_t signal_quit;

static void observer_instruction_begin(void *opaque, uint32_t pc,
                                       uint32_t pa, uint32_t word,
                                       const Registers *state) {
    (void)opaque;
    debugger_instruction_begin(pc, pa, word, state);
}

static void observer_instruction_end(void *opaque, const Registers *state) {
    (void)opaque;
    debugger_instruction_end(state);
}

static void observer_exception(void *opaque, const Registers *state,
                               uint32_t exc_info, uint8_t exc_code,
                               VectorClass vector_class) {
    (void)opaque;
    debugger_exception(state, exc_info, exc_code, vector_class);
}
static const int handled_signals[HANDLED_SIGNAL_COUNT] = {
    SIGINT, SIGTERM, SIGPIPE,
};

static const char *const gpr_names[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

#ifdef MIPSEL_EMU_HAVE_CURSES
static uint64_t monotonic_ns(void) {
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}
#endif

static void handle_signal(int signo) {
    (void)signo;
    signal_quit = 1;
}

static int install_signal_handlers(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    if (sigemptyset(&action.sa_mask) != 0) return -1;

    for (size_t i = 0; i < HANDLED_SIGNAL_COUNT; ++i) {
        if (sigaction(handled_signals[i], &action,
                      &debugger.previous_signal_actions[i]) != 0) {
            int saved_errno = errno;
            while (debugger.signal_action_count != 0) {
                size_t index = --debugger.signal_action_count;
                (void)sigaction(handled_signals[index],
                                &debugger.previous_signal_actions[index],
                                NULL);
            }
            errno = saved_errno;
            return -1;
        }
        ++debugger.signal_action_count;
    }
    return 0;
}

static void restore_signal_handlers(void) {
    while (debugger.signal_action_count != 0) {
        size_t index = --debugger.signal_action_count;
        (void)sigaction(handled_signals[index],
                        &debugger.previous_signal_actions[index], NULL);
    }
}

static void snapshot_registers(RegisterSnapshot *snapshot,
                               const Registers *state) {
    memcpy(snapshot->gpr, state->gpr, sizeof(snapshot->gpr));
    snapshot->hi = state->hi;
    snapshot->lo = state->lo;
    snapshot->status = state->cp0.byname.cp0r12_t.cp0r12_n.Status;
    snapshot->cause = state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
    snapshot->epc = state->cp0.byname.cp0r14_t.cp0r14_n.EPC;
    snapshot->badvaddr = state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr;
}

static void ring_add(char lines[][DEBUG_LINE_LENGTH], size_t capacity,
                     size_t *head, size_t *count, const char *line) {
    snprintf(lines[*head], DEBUG_LINE_LENGTH, "%s", line);
    *head = (*head + 1u) % capacity;
    if (*count < capacity) ++*count;
}

#if MIPSEL_EMU_ENABLE_CONSOLE
static bool monitor_target_halted(void *opaque) {
    const vmstate_t *vm = opaque;
    return vm && vm->state == STEPPING && vm->steps == 0;
}

static bool monitor_bus_read(void *opaque, uint32_t pa, unsigned width,
                             uint32_t *value) {
    (void)opaque;
    return platform_bus_read(pa, width, value);
}

static bool monitor_bus_write(void *opaque, uint32_t pa, unsigned width,
                              uint32_t value) {
    (void)opaque;
    return platform_bus_write(pa, width, value);
}

static void monitor_commit_line(void) {
    debugger.monitor_current[debugger.monitor_current_length] = '\0';
    ring_add(debugger.monitor_lines, MONITOR_LINE_CAPACITY,
             &debugger.monitor_head, &debugger.monitor_count,
             debugger.monitor_current);
    debugger.monitor_current_length = 0;
    debugger.monitor_current[0] = '\0';
}

static void monitor_output(void *opaque, const char *bytes, size_t length) {
    (void)opaque;

    for (size_t index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char)bytes[index];
        if (byte == '\r') continue;
        if (byte == '\n') {
            monitor_commit_line();
            continue;
        }
        if (debugger.monitor_current_length >= DEBUG_LINE_LENGTH - 1u)
            monitor_commit_line();
        debugger.monitor_current[debugger.monitor_current_length++] =
            (char)((byte >= 0x20u && byte <= 0x7eu) || byte == '\t'
                       ? byte : '.');
        debugger.monitor_current[debugger.monitor_current_length] = '\0';
    }
    debugger.monitor_scroll = 0;
    debugger.force_redraw = true;
}

#ifdef MIPSEL_EMU_HAVE_CURSES
static void monitor_record_command(void) {
    char line[DEBUG_LINE_LENGTH];

    if (mipsel_console_input_length(&debugger.monitor) == 0u) return;
    if (debugger.monitor_current_length != 0u) monitor_commit_line();
    (void)snprintf(line, sizeof(line), "mipsel-emu> %s",
                   mipsel_console_input(&debugger.monitor));
    ring_add(debugger.monitor_lines, MONITOR_LINE_CAPACITY,
             &debugger.monitor_head, &debugger.monitor_count, line);
    debugger.monitor_scroll = 0;
}
#endif
#endif

static void trace_add(const char *format, ...) {
    char line[DEBUG_LINE_LENGTH];
    va_list args;

    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    ring_add(debugger.trace, TRACE_CAPACITY, &debugger.trace_head,
             &debugger.trace_count, line);

    if (debugger.trace_file) {
        fprintf(debugger.trace_file, "%s\n", line);
    }
}

static void trace_exception(const char *line) {
    trace_add("%010" PRIu64 " %s", ++debugger.sequence, line);
}

static const char *exception_name(uint8_t code) {
    switch (code) {
        case EXC_RESET: return "Reset";
        case EXC_SRES: return "SoftReset";
        case EXC_INT: return "Interrupt";
        case EXC_MOD: return "TLBModified";
        case EXC_TLBL: return "TLBLoad";
        case EXC_TLBS: return "TLBStore";
        case EXC_AdEL: return "AddressLoad";
        case EXC_AdES: return "AddressStore";
        case EXC_IBE: return "BusFetch";
        case EXC_DBE: return "BusData";
        case EXC_SC: return "Syscall";
        case EXC_BP: return "Breakpoint";
        case EXC_RI: return "ReservedInstruction";
        case EXC_CpU: return "CoprocessorUnusable";
        case EXC_Ov: return "Overflow";
        case EXC_Tr: return "Trap";
        case EXC_FPE: return "FloatingPoint";
        case EXC_C2E: return "Coprocessor2";
        case EXC_DSP: return "DSP";
        case EXC_WATCH: return "Watch";
        case EXC_MCheck: return "MachineCheck";
        case EXC_THR: return "Thread";
        case EXC_CAH: return "CacheError";
        default: return "Unknown";
    }
}

static const char *vector_name(VectorClass vector_class) {
    switch (vector_class) {
        case MIPS_VECTOR_TLB_REFILL: return "tlb-refill";
        case MIPS_VECTOR_INTERRUPT: return "interrupt";
        case MIPS_VECTOR_CACHE_ERROR: return "cache-error";
        case MIPS_VECTOR_RESET: return "reset";
        case MIPS_VECTOR_GENERAL:
        default: return "general";
    }
}

static void uart_commit_line(void) {
    size_t slot = debugger.uart_head;

    debugger.uart_current[debugger.uart_current_length] = '\0';
    snprintf(debugger.uart_lines[slot], UART_LINE_LENGTH, "%s",
             debugger.uart_current);
    debugger.uart_head = (slot + 1u) % UART_LINE_CAPACITY;
    if (debugger.uart_count < UART_LINE_CAPACITY) ++debugger.uart_count;
    debugger.uart_current_length = 0;
    debugger.uart_current[0] = '\0';
}

static void flush_uart_input(void) {
    while (debugger.uart_input_count != 0 &&
           platform_uart_can_receive()) {
        uint8_t byte = debugger.uart_input[debugger.uart_input_head];
        if (!platform_uart_receive(byte)) break;
        debugger.uart_input_head =
            (debugger.uart_input_head + 1u) % debugger.uart_input_capacity;
        --debugger.uart_input_count;
    }
}

static bool grow_uart_input(void) {
    size_t new_capacity = debugger.uart_input_capacity
        ? debugger.uart_input_capacity * 2u : UART_INPUT_INITIAL_CAPACITY;
    uint8_t *new_input;

    if (new_capacity > UART_INPUT_MAX_CAPACITY)
        new_capacity = UART_INPUT_MAX_CAPACITY;
    if (new_capacity <= debugger.uart_input_capacity) return false;
    new_input = malloc(new_capacity);
    if (!new_input) return false;
    for (size_t i = 0; i < debugger.uart_input_count; ++i) {
        new_input[i] = debugger.uart_input[
            (debugger.uart_input_head + i) % debugger.uart_input_capacity];
    }
    free(debugger.uart_input);
    debugger.uart_input = new_input;
    debugger.uart_input_capacity = new_capacity;
    debugger.uart_input_head = 0;
    return true;
}

static bool queue_uart_input(uint8_t byte) {
    size_t tail;

    flush_uart_input();
    if (debugger.uart_input_count == 0 && platform_uart_can_receive() &&
        platform_uart_receive(byte)) {
        return true;
    }
    if (debugger.uart_input_count == debugger.uart_input_capacity &&
        !grow_uart_input()) {
        /* The bounded host queue lost a byte: expose that as UART OE. */
        (void)platform_uart_receive(byte);
        return false;
    }
    tail = (debugger.uart_input_head + debugger.uart_input_count) %
           debugger.uart_input_capacity;
    debugger.uart_input[tail] = byte;
    ++debugger.uart_input_count;
    return true;
}

static void poll_headless_uart_input(void) {
    struct pollfd input = {
        .fd = STDIN_FILENO,
        .events = POLLIN,
    };
    uint8_t bytes[256];
    size_t available;
    ssize_t count;
    bool overflow_notified = false;

    if (debugger.stdin_closed) return;
    flush_uart_input();
    available = UART_INPUT_MAX_CAPACITY - debugger.uart_input_count;
    if (available == 0 && !debugger.stdin_termios_saved) return;
    if (poll(&input, 1, 0) <= 0) return;
    if (!(input.revents & (POLLIN | POLLHUP))) return;
    if (debugger.stdin_termios_saved || available > sizeof(bytes))
        available = sizeof(bytes);
    count = read(STDIN_FILENO, bytes, available);
    if (count == 0) {
        debugger.stdin_closed = true;
        return;
    }
    if (count < 0) return;
    for (ssize_t i = 0; i < count; ++i) {
        uint8_t byte = bytes[i] == '\n' ? '\r' : bytes[i];

        if (debugger.stdin_termios_saved) {
            if (debugger.headless_escape) {
                debugger.headless_escape = false;
                if (byte == 'q' || byte == 'Q') {
                    debugger.quit = true;
                    break;
                }
                /* Ctrl-] Ctrl-] sends one literal Ctrl-] to the guest. */
            } else if (byte == 29u) {
                debugger.headless_escape = true;
                continue;
            }
        }
        if (!queue_uart_input(byte)) {
            if (!debugger.stdin_termios_saved) break;
            if (!overflow_notified) {
                (void)write(STDERR_FILENO, "\a", 1);
                overflow_notified = true;
            }
        }
    }
}

int debugger_init(const DebuggerConfig *config, Registers *state,
                  vmstate_t *vm) {
    unsigned int refresh_hz = config && config->refresh_hz
                                  ? config->refresh_hz : 30u;
#ifndef MIPSEL_EMU_HAVE_CURSES
    (void)vm;
#endif

    memset(&debugger, 0, sizeof(debugger));
    signal_quit = 0;
    debugger.initialized = true;
    debugger.capture_events = config &&
        (config->enable_tui || config->trace_path != NULL);
    debugger.refresh_hz = refresh_hz;

    if (config && config->enable_tui && config->trace_path &&
        strcmp(config->trace_path, "-") == 0) {
        fprintf(stderr,
                "--tui cannot share the terminal with --trace -; "
                "use a trace file\n");
        debugger_shutdown();
        return -1;
    }

    if (install_signal_handlers() != 0) {
        fprintf(stderr, "cannot install terminal signal handlers: %s\n",
                strerror(errno));
        debugger_shutdown();
        return -1;
    }

    if (config && config->trace_path) {
        if (strcmp(config->trace_path, "-") == 0) {
            debugger.trace_file = stderr;
        } else {
            debugger.trace_file = fopen(config->trace_path, "w");
            if (!debugger.trace_file) {
                fprintf(stderr, "cannot open trace '%s': %s\n",
                        config->trace_path, strerror(errno));
                debugger_shutdown();
                return -1;
            }
            debugger.trace_file_owned = true;
            (void)setvbuf(debugger.trace_file, NULL, _IOLBF, 0);
        }
    }

    snapshot_registers(&debugger.before, state);

#if MIPSEL_EMU_ENABLE_CONSOLE
    if (!mipsel_console_init(&debugger.monitor,
                             &(mipsel_console_config_t) {
                                 .registers = state,
                                 .halted = monitor_target_halted,
                                 .bus_read = monitor_bus_read,
                                 .bus_write = monitor_bus_write,
                                 .target_opaque = vm,
                                 .output = monitor_output,
                             })) {
        fprintf(stderr, "cannot initialize target monitor\n");
        debugger_shutdown();
        return -1;
    }
    monitor_output(NULL, "Monitor ready; type 'help' for commands.\n", 41u);
#endif

    if (config && config->enable_tui) {
#ifdef MIPSEL_EMU_HAVE_CURSES
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            fprintf(stderr, "--tui requires terminal stdin and stdout\n");
            debugger_shutdown();
            return -1;
        }
        debugger.curses_screen = newterm(NULL, stdout, stdin);
        if (!debugger.curses_screen) {
            fprintf(stderr, "failed to initialize ncurses (check TERM)\n");
            debugger_shutdown();
            return -1;
        }
        (void)set_term(debugger.curses_screen);
        if (raw() == ERR || noecho() == ERR || nonl() == ERR ||
            keypad(stdscr, true) == ERR) {
            fprintf(stderr, "failed to configure ncurses terminal mode\n");
            debugger_shutdown();
            return -1;
        }
        timeout(0);
        (void)curs_set(0);
        if (has_colors()) {
            start_color();
            use_default_colors();
            init_pair(1, COLOR_CYAN, -1);
            init_pair(2, COLOR_YELLOW, -1);
            init_pair(3, COLOR_RED, -1);
            init_pair(4, COLOR_GREEN, -1);
        }
        debugger.tui = true;
        debugger.force_redraw = true;
        if (config->start_paused) {
            vm->state = STEPPING;
            vm->steps = 0;
        }
#else
        fprintf(stderr, "this build has no ncurses TUI support\n");
        debugger_shutdown();
        return -1;
#endif
    } else if (isatty(STDIN_FILENO)) {
        struct termios input_mode;

        if (tcgetattr(STDIN_FILENO, &debugger.stdin_termios) != 0) {
            fprintf(stderr, "cannot read stdin terminal mode: %s\n",
                    strerror(errno));
            debugger_shutdown();
            return -1;
        }
        input_mode = debugger.stdin_termios;
        input_mode.c_lflag &= (tcflag_t)~(ICANON | ECHO | IEXTEN | ISIG);
        input_mode.c_iflag &= (tcflag_t)~(IGNBRK | BRKINT | PARMRK |
            ISTRIP | INLCR | IGNCR | ICRNL | INPCK | IXON | IXOFF);
        input_mode.c_cflag &= (tcflag_t)~(CSIZE | PARENB);
        input_mode.c_cflag |= CS8;
        input_mode.c_cc[VMIN] = 0;
        input_mode.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &input_mode) != 0) {
            fprintf(stderr, "cannot configure stdin for UART input: %s\n",
                    strerror(errno));
            debugger_shutdown();
            return -1;
        }
        debugger.stdin_termios_saved = true;
    }

    if (debugger.capture_events)
        trace_add("# mipsel-emu trace: pc physical raw disassembly changes");
    mipsel_emu_observer_set(&(mipsel_emu_observer_t) {
        .instruction_begin = observer_instruction_begin,
        .instruction_end = observer_instruction_end,
        .exception = observer_exception,
    });
    return 0;
}

void debugger_shutdown(void) {
    if (!debugger.initialized) return;

    mipsel_emu_observer_set(NULL);

#ifdef MIPSEL_EMU_HAVE_CURSES
    if (debugger.curses_screen) {
        if (debugger.tui) timeout(-1);
        endwin();
        delscreen(debugger.curses_screen);
    }
#endif
    if (debugger.stdin_termios_saved) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH,
                        &debugger.stdin_termios);
    }
    if (debugger.trace_file_owned && debugger.trace_file) {
        fclose(debugger.trace_file);
    }
    free(debugger.uart_input);
    restore_signal_handlers();
    memset(&debugger, 0, sizeof(debugger));
}

void debugger_instruction_begin(uint32_t pc, uint32_t pa, uint32_t word,
                                const Registers *state) {
    if (!debugger.initialized || !debugger.capture_events) return;
    debugger.in_instruction = true;
    debugger.instruction_pc = pc;
    debugger.instruction_pa = pa;
    debugger.instruction_word = word;
    debugger.pending_instruction_exception = false;
    snapshot_registers(&debugger.before, state);
}

void debugger_instruction_end(const Registers *state) {
    char disassembly[96];
    char changes[80];
    size_t used = 0;

    if (!debugger.initialized || !debugger.capture_events ||
        !debugger.in_instruction) return;
    debugger.in_instruction = false;
    debugger.changed_gpr = 0;
    changes[0] = '\0';

    for (unsigned int i = 0; i < 32; ++i) {
        int written;
        if (state->gpr[i] == debugger.before.gpr[i]) continue;
        debugger.changed_gpr |= UINT32_C(1) << i;
        if (used >= sizeof(changes) - 1u) break;
        written = snprintf(changes + used, sizeof(changes) - used,
                           "%s%s=%08" PRIx32, used ? " " : "",
                           gpr_names[i], state->gpr[i]);
        if (written < 0) break;
        if ((size_t)written >= sizeof(changes) - used) {
            used = sizeof(changes) - 1u;
            break;
        }
        used += (size_t)written;
    }
    if (state->hi != debugger.before.hi && used < sizeof(changes) - 1u) {
        int written = snprintf(changes + used, sizeof(changes) - used,
                               "%sHI=%08" PRIx32, used ? " " : "",
                               state->hi);
        if (written > 0) used += (size_t)written;
    }
    if (state->lo != debugger.before.lo && used < sizeof(changes) - 1u) {
        (void)snprintf(changes + used, sizeof(changes) - used,
                       "%sLO=%08" PRIx32, used ? " " : "", state->lo);
    }

    mips_disassemble(debugger.instruction_pc, debugger.instruction_word,
                     disassembly, sizeof(disassembly));
    trace_add("%010" PRIu64 " %08" PRIx32 "[%08" PRIx32
              "] %08" PRIx32 "  %-36s%s%s",
              ++debugger.sequence, debugger.instruction_pc,
              debugger.instruction_pa, debugger.instruction_word,
              disassembly, changes[0] ? " ; " : "", changes);
    if (debugger.pending_instruction_exception) {
        trace_exception(debugger.instruction_exception);
        debugger.pending_instruction_exception = false;
    }
}

void debugger_exception(const Registers *state, uint32_t exc_info,
                        uint8_t exc_code, VectorClass vector_class) {
    char line[DEBUG_LINE_LENGTH];

    if (!debugger.initialized || !debugger.capture_events) return;
    snprintf(line, sizeof(line),
             "! %s code=%02x pc=%08" PRIx32 " info=%08" PRIx32
             " EPC=%08" PRIx32 " Cause=%08" PRIx32
             " -> %08" PRIx32 " (%s%s)",
             exception_name(exc_code), exc_code, state->pc, exc_info,
             state->cp0.byname.cp0r14_t.cp0r14_n.EPC,
             state->cp0.byname.cp0r13_t.cp0r13_n.Cause, state->next_pc,
             vector_name(vector_class), CAUSE_BD(state) ? ", BD" : "");
    ring_add(debugger.exceptions, EXCEPTION_CAPACITY,
             &debugger.exception_head, &debugger.exception_count, line);
    if (debugger.in_instruction) {
        if (debugger.pending_instruction_exception)
            trace_exception(debugger.instruction_exception);
        snprintf(debugger.instruction_exception,
                 sizeof(debugger.instruction_exception), "%s", line);
        debugger.pending_instruction_exception = true;
    } else {
        trace_exception(line);
    }
    debugger.force_redraw = true;
}

void debugger_uart_tx(uint8_t byte) {
    if (!debugger.initialized || !debugger.tui) {
        if (fputc(byte, stdout) == EOF || fflush(stdout) == EOF)
            debugger.quit = true;
        return;
    }

    if (byte == '\r') return;
    if (byte == '\n') {
        uart_commit_line();
    } else {
        char shown = (char)(isprint((unsigned char)byte) || byte == '\t'
                                ? byte : '.');
        if (debugger.uart_current_length >= UART_LINE_LENGTH - 1u) {
            uart_commit_line();
        }
        debugger.uart_current[debugger.uart_current_length++] = shown;
        debugger.uart_current[debugger.uart_current_length] = '\0';
    }
    debugger.force_redraw = true;
}

void debugger_board_reset(const Registers *state) {
    if (!debugger.initialized) return;
    debugger.uart_input_head = 0;
    debugger.uart_input_count = 0;
    debugger.headless_escape = false;
    debugger.uart_focus = false;
    debugger.in_instruction = false;
    debugger.pending_instruction_exception = false;
    debugger.changed_gpr = 0;
    snapshot_registers(&debugger.before, state);
#if MIPSEL_EMU_ENABLE_CONSOLE
    debugger.monitor_focus = false;
    mipsel_console_reset(&debugger.monitor);
    monitor_output(NULL, "# target reset\n", 15u);
#endif
    if (debugger.capture_events) {
        trace_add("%010" PRIu64 " # board reset -> PC=%08" PRIx32,
                  ++debugger.sequence, state->pc);
    }
    debugger.force_redraw = true;
}

#ifdef MIPSEL_EMU_HAVE_CURSES
static void put_clipped(int y, int x, int width, const char *text,
                        int attributes) {
    int rows, columns;

    getmaxyx(stdscr, rows, columns);
    if (y < 0 || y >= rows || x < 0 || x >= columns || width <= 0) return;
    if (x + width > columns) width = columns - x;
    if (attributes) attron(attributes);
    mvaddnstr(y, x, text, width);
    if (attributes) attroff(attributes);
}

static void draw_box(int y, int x, int height, int width,
                     const char *title, int title_attributes) {
    int rows, columns;

    getmaxyx(stdscr, rows, columns);
    if (height < 2 || width < 2 || y >= rows || x >= columns) return;
    if (y + height > rows) height = rows - y;
    if (x + width > columns) width = columns - x;
    if (height < 2 || width < 2) return;
    mvhline(y, x + 1, ACS_HLINE, width - 2);
    mvhline(y + height - 1, x + 1, ACS_HLINE, width - 2);
    mvvline(y + 1, x, ACS_VLINE, height - 2);
    mvvline(y + 1, x + width - 1, ACS_VLINE, height - 2);
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + width - 1, ACS_URCORNER);
    mvaddch(y + height - 1, x, ACS_LLCORNER);
    mvaddch(y + height - 1, x + width - 1, ACS_LRCORNER);
    if (title && width > 4) put_clipped(y, x + 2, width - 4, title,
                                        title_attributes | A_BOLD);
}

static void draw_ring(char lines[][DEBUG_LINE_LENGTH], size_t capacity,
                      size_t head, size_t count, int y, int x,
                      int height, int width, unsigned int scroll,
                      int attributes) {
    size_t visible;
    size_t end;
    size_t start;

    if (height <= 2 || width <= 2 || count == 0) return;
    visible = (size_t)(height - 2);
    end = count > scroll ? count - scroll : 0;
    start = end > visible ? end - visible : 0;
    for (size_t logical = start; logical < end; ++logical) {
        size_t oldest = (head + capacity - count) % capacity;
        size_t slot = (oldest + logical) % capacity;
        int row = y + 1 + (int)(logical - start);
        put_clipped(row, x + 1, width - 2, lines[slot], attributes);
    }
}

static void draw_uart(int y, int x, int height, int width) {
    size_t visible;
    size_t completed_visible;
    size_t start;

    if (height <= 2 || width <= 2) return;
    visible = (size_t)(height - 2);
    completed_visible = debugger.uart_count;
    if (debugger.uart_current_length && completed_visible >= visible) {
        completed_visible = visible - 1u;
    } else if (completed_visible > visible) {
        completed_visible = visible;
    }
    start = debugger.uart_count - completed_visible;
    for (size_t i = 0; i < completed_visible; ++i) {
        size_t oldest = (debugger.uart_head + UART_LINE_CAPACITY -
                         debugger.uart_count) % UART_LINE_CAPACITY;
        size_t slot = (oldest + start + i) % UART_LINE_CAPACITY;
        put_clipped(y + 1 + (int)i, x + 1, width - 2,
                    debugger.uart_lines[slot], COLOR_PAIR(4));
    }
    if (debugger.uart_current_length && visible > 0) {
        put_clipped(y + 1 + (int)completed_visible, x + 1, width - 2,
                    debugger.uart_current, COLOR_PAIR(4));
    }
}

#if MIPSEL_EMU_ENABLE_CONSOLE
static bool monitor_window_usable(void) {
    int rows, columns;

    getmaxyx(stdscr, rows, columns);
    return rows >= 6 && columns >= 4;
}

static size_t monitor_visible_line_count(void) {
    int rows, columns;

    getmaxyx(stdscr, rows, columns);
    (void)columns;
    return rows > 5 ? (size_t)(rows - 5) : 0u;
}

static void format_monitor_prompt(char *prompt, size_t prompt_size,
                                  int inner_width) {
    static const char prefix[] = "mipsel-emu> ";
    const char *input = mipsel_console_input(&debugger.monitor);
    size_t input_length = mipsel_console_input_length(&debugger.monitor);
    size_t prefix_length = strlen(prefix);
    size_t available;

    if (!prompt || prompt_size == 0u) return;
    prompt[0] = '\0';
    if (inner_width <= 0) return;
    if (input_length == 0u) {
        (void)snprintf(prompt, prompt_size, "%s", prefix);
        return;
    }
    if ((size_t)inner_width <= prefix_length) {
        size_t available = (size_t)inner_width;
        if (available == 1u) {
            (void)snprintf(prompt, prompt_size, "<");
        } else {
            size_t shown = available - 1u;
            const char *tail = input + (input_length > shown
                                             ? input_length - shown : 0u);
            (void)snprintf(prompt, prompt_size, "<%s", tail);
        }
        return;
    }

    available = (size_t)inner_width - prefix_length;
    if (input_length <= available) {
        (void)snprintf(prompt, prompt_size, "%s%s", prefix, input);
    } else if (available == 1u) {
        (void)snprintf(prompt, prompt_size, "%s<", prefix);
    } else {
        const char *tail = input + input_length - (available - 1u);
        (void)snprintf(prompt, prompt_size, "%s<%s", prefix, tail);
    }
}

static void draw_monitor(int y, int x, int height, int width,
                         bool target_halted) {
    char prompt[MIPSEL_EMU_CONSOLE_LINE_SIZE + 16u];
    int input_row;
    int cursor_column;

    if (height < 4 || width < 4) return;
    draw_box(y, x, height, width,
             target_halted
                 ? "Monitor [PAUSED] - Esc/F3 close, PgUp/PgDn scroll"
                 : "Monitor [TARGET RUNNING] - Esc/F3 close",
             COLOR_PAIR(2));
    draw_ring(debugger.monitor_lines, MONITOR_LINE_CAPACITY,
              debugger.monitor_head, debugger.monitor_count,
              y, x, height - 1, width, debugger.monitor_scroll, 0);
    if (target_halted) {
        format_monitor_prompt(prompt, sizeof(prompt), width - 2);
    } else {
        (void)snprintf(prompt, sizeof(prompt),
                       "target running; monitor input is locked");
    }
    input_row = y + height - 2;
    put_clipped(input_row, x + 1, width - 2, prompt,
                COLOR_PAIR(2) | A_BOLD);
    cursor_column = x + 1 + (int)strlen(prompt);
    if (cursor_column >= x + width - 1) cursor_column = x + width - 2;
    if (target_halted && cursor_column >= x + 1) {
        (void)curs_set(1);
        (void)move(input_row, cursor_column);
    }
}
#endif

static const char *cpu_state_name(const vmstate_t *vm) {
    if (vm->state == RUNNING) return "RUNNING";
    if (vm->state == RESET) return "RESET";
    if (vm->state == STEPPING && vm->steps) return "STEPPING";
    return "PAUSED";
}

static void draw_registers(const Registers *state, int y, int x,
                           int height, int width) {
    int inner = width - 2;
    int column_width = inner / 2;

    for (unsigned int row = 0; row < 16 && (int)row < height - 2; ++row) {
        for (unsigned int column = 0; column < 2; ++column) {
            unsigned int reg = row + column * 16u;
            char value[40];
            int attr = (debugger.changed_gpr & (UINT32_C(1) << reg))
                           ? COLOR_PAIR(2) | A_BOLD : 0;
            snprintf(value, sizeof(value), "$%02u %-4s %08" PRIx32,
                     reg, gpr_names[reg], state->gpr[reg]);
            put_clipped(y + 1 + (int)row, x + 1 + (int)column * column_width,
                        column_width, value, attr);
        }
    }
}

static void draw_cp0(const Registers *state, int y, int x,
                     int height, int width) {
    char values[14][64];
    size_t count = 0;

#define CP0_VALUE(label, value) \
    snprintf(values[count++], sizeof(values[0]), "%-9s %08" PRIx32, \
             (label), (uint32_t)(value))
    CP0_VALUE("PC", state->pc);
    CP0_VALUE("next PC", state->next_pc);
    CP0_VALUE("HI", state->hi);
    CP0_VALUE("LO", state->lo);
    CP0_VALUE("Status", state->cp0.byname.cp0r12_t.cp0r12_n.Status);
    CP0_VALUE("Cause", state->cp0.byname.cp0r13_t.cp0r13_n.Cause);
    CP0_VALUE("EPC", state->cp0.byname.cp0r14_t.cp0r14_n.EPC);
    CP0_VALUE("BadVAddr", state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr);
    CP0_VALUE("Count", state->cp0.byname.cp0r9_t.cp0r9_n.Count);
    CP0_VALUE("Compare", state->cp0.byname.cp0r11_t.cp0r11_n.Compare);
    CP0_VALUE("EntryHi", state->cp0.byname.cp0r10_t.cp0r10_n.EntryHi);
    CP0_VALUE("EntryLo0", state->cp0.byname.cp0r2_t.cp0r2_n.EntryLo0);
    CP0_VALUE("EntryLo1", state->cp0.byname.cp0r3_t.cp0r3_n.EntryLo1);
    CP0_VALUE("Random", state->cp0.byname.cp0r1_t.cp0r1_n.Random);
#undef CP0_VALUE

    for (size_t i = 0; i < count && (int)i < height - 2; ++i) {
        put_clipped(y + 1 + (int)i, x + 1, width - 2, values[i], 0);
    }
}

static void draw_full_tui(const Registers *state, const vmstate_t *vm) {
    int rows, columns;
    int top_y = 2;
    int content_height;
    int top_height;
    int left_width;
    char header[256];

    getmaxyx(stdscr, rows, columns);
    erase();
    (void)curs_set(0);
    snprintf(header, sizeof(header),
             " mipsel-emu  %-8s  ticks=%" PRIu64
             "  PC=%08" PRIx32 "%s",
             cpu_state_name(vm), vm->ticks, state->pc,
             debugger.uart_focus ? "  [UART INPUT: Ctrl-] to leave]" :
#if MIPSEL_EMU_ENABLE_CONSOLE
             debugger.monitor_focus
                 ? (vm && vm->state == STEPPING && vm->steps == 0
                        ? "  [MONITOR INPUT: Esc/F3 to leave]"
                        : "  [MONITOR LOCKED: Esc/F3 to leave]") :
#endif
             "");
    put_clipped(0, 0, columns, header, COLOR_PAIR(1) | A_BOLD);
    put_clipped(1, 0, columns,
                " Space run/pause  s step  n +100  r reset  F2 UART  "
#if MIPSEL_EMU_ENABLE_CONSOLE
                "F3/: Monitor  "
#endif
                "Up/Down trace  Tab page  q quit",
                A_DIM);

#if MIPSEL_EMU_ENABLE_CONSOLE
    if (debugger.monitor_focus) {
        draw_monitor(top_y, 0, rows - top_y, columns,
                     monitor_target_halted((void *)vm));
        refresh();
        return;
    }
#endif

    if (rows < 30 || columns < 100) {
        const char *titles[] = {"Registers", "Instruction trace",
                                "Exceptions", "MMIO UART"};
        int height = rows - top_y;
        draw_box(top_y, 0, height, columns, titles[debugger.active_page],
                 COLOR_PAIR(1));
        switch (debugger.active_page) {
            case 0:
                draw_registers(state, top_y, 0, height, columns);
                break;
            case 1:
                draw_ring(debugger.trace, TRACE_CAPACITY,
                          debugger.trace_head, debugger.trace_count,
                          top_y, 0, height, columns, debugger.trace_scroll, 0);
                break;
            case 2:
                draw_ring(debugger.exceptions, EXCEPTION_CAPACITY,
                          debugger.exception_head, debugger.exception_count,
                          top_y, 0, height, columns, 0, COLOR_PAIR(3));
                break;
            default:
                draw_uart(top_y, 0, height, columns);
                break;
        }
        refresh();
        return;
    }

    content_height = rows - top_y;
    top_height = content_height > 34 ? 19 : content_height / 2;
    if (top_height < 12) top_height = 12;
    left_width = columns * 3 / 5;

    draw_box(top_y, 0, top_height, left_width, "GPR", COLOR_PAIR(1));
    draw_registers(state, top_y, 0, top_height, left_width);
    draw_box(top_y, left_width, top_height, columns - left_width,
             "Control / CP0", COLOR_PAIR(1));
    draw_cp0(state, top_y, left_width, top_height, columns - left_width);

    draw_box(top_y + top_height, 0, content_height - top_height, left_width,
             debugger.trace_scroll ? "Instruction trace [scrolled]"
                                   : "Instruction trace",
             COLOR_PAIR(1));
    draw_ring(debugger.trace, TRACE_CAPACITY, debugger.trace_head,
              debugger.trace_count, top_y + top_height, 0,
              content_height - top_height, left_width,
              debugger.trace_scroll, 0);

    {
        int right_height = content_height - top_height;
        int exception_height = right_height / 2;
        int right_width = columns - left_width;
        if (exception_height < 4) exception_height = 4;
        draw_box(top_y + top_height, left_width, exception_height,
                 right_width, "Exceptions", COLOR_PAIR(3));
        draw_ring(debugger.exceptions, EXCEPTION_CAPACITY,
                  debugger.exception_head, debugger.exception_count,
                  top_y + top_height, left_width, exception_height,
                  right_width, 0, COLOR_PAIR(3));
        draw_box(top_y + top_height + exception_height, left_width,
                 right_height - exception_height, right_width,
                 debugger.uart_focus ? "MMIO UART [INPUT]" : "MMIO UART",
                 debugger.uart_focus ? COLOR_PAIR(2) : COLOR_PAIR(4));
        draw_uart(top_y + top_height + exception_height, left_width,
                  right_height - exception_height, right_width);
    }
    refresh();
}

static void clear_active_page(void) {
    switch (debugger.active_page) {
        case 1:
            debugger.trace_head = debugger.trace_count = 0;
            debugger.trace_scroll = 0;
            break;
        case 2:
            debugger.exception_head = debugger.exception_count = 0;
            break;
        case 3:
            debugger.uart_head = debugger.uart_count = 0;
            debugger.uart_current_length = 0;
            debugger.uart_current[0] = '\0';
            break;
        default:
            break;
    }
}

#if MIPSEL_EMU_ENABLE_CONSOLE
static void handle_monitor_result(mipsel_console_result_t result,
                                  const Registers *state) {
    if (result == MIPSEL_CONSOLE_STATE_CHANGED) {
        debugger.changed_gpr = 0;
        snapshot_registers(&debugger.before, state);
    }
    if (result != MIPSEL_CONSOLE_NO_COMMAND)
        debugger.monitor_scroll = 0;
}
#endif

static bool handle_key(int key, Registers *state, vmstate_t *vm) {
#if !MIPSEL_EMU_ENABLE_CONSOLE
    (void)state;
#endif
#if MIPSEL_EMU_ENABLE_CONSOLE
    if (debugger.monitor_focus) {
        uint8_t byte;
        mipsel_console_result_t result;

        if (!monitor_target_halted(vm)) {
            mipsel_console_cancel_input(&debugger.monitor);
            if (key == 27 || key == KEY_F(3)) {
                debugger.monitor_focus = false;
            } else {
                /* Keep focus quarantined until the user explicitly leaves. */
                (void)flushinp();
                beep();
            }
            debugger.force_redraw = true;
            return false;
        }
        if (!monitor_window_usable()) {
            mipsel_console_cancel_input(&debugger.monitor);
            if (key == 27 || key == KEY_F(3)) {
                debugger.monitor_focus = false;
            } else {
                /* Do not accept blind input while the monitor is hidden. */
                (void)flushinp();
                beep();
            }
            debugger.force_redraw = true;
            return false;
        }
        if (key == 27 || key == KEY_F(3)) {
            debugger.monitor_focus = false;
            mipsel_console_cancel_input(&debugger.monitor);
        } else if (key == KEY_PPAGE || key == KEY_UP) {
            unsigned amount = key == KEY_PPAGE ? 8u : 1u;
            size_t visible = monitor_visible_line_count();
            size_t maximum = debugger.monitor_count > visible
                                 ? debugger.monitor_count - visible : 0u;
            size_t next = (size_t)debugger.monitor_scroll + amount;
            debugger.monitor_scroll = (unsigned int)(next > maximum
                                                       ? maximum : next);
        } else if (key == KEY_NPAGE || key == KEY_DOWN) {
            unsigned amount = key == KEY_NPAGE ? 8u : 1u;
            debugger.monitor_scroll = debugger.monitor_scroll > amount
                                          ? debugger.monitor_scroll - amount
                                          : 0;
        } else if (key == KEY_BACKSPACE || key == 0x7f || key == '\b') {
            byte = 0x7f;
            result = mipsel_console_feed(&debugger.monitor, &byte, 1);
            handle_monitor_result(result, state);
        } else if (key == KEY_ENTER || key == '\n' || key == '\r') {
            monitor_record_command();
            byte = '\n';
            result = mipsel_console_feed(&debugger.monitor, &byte, 1);
            handle_monitor_result(result, state);
        } else if (key == KEY_RESIZE) {
            size_t visible = monitor_visible_line_count();
            size_t maximum = debugger.monitor_count > visible
                                 ? debugger.monitor_count - visible : 0u;
            if ((size_t)debugger.monitor_scroll > maximum)
                debugger.monitor_scroll = (unsigned int)maximum;
        } else if (key == '\t') {
            byte = ' ';
            result = mipsel_console_feed(&debugger.monitor, &byte, 1);
            handle_monitor_result(result, state);
        } else if (key == 0x15 || (key >= 0x20 && key <= 0x7e)) {
            byte = (uint8_t)key;
            result = mipsel_console_feed(&debugger.monitor, &byte, 1);
            handle_monitor_result(result, state);
        } else {
            beep();
        }
        debugger.force_redraw = true;
        return true;
    }
#endif

    if (debugger.uart_focus) {
        if (key == 29 || key == KEY_F(2)) {
            debugger.uart_focus = false;
        } else if (key == KEY_BACKSPACE) {
            if (!queue_uart_input(0x7fu)) beep();
        } else if (key == KEY_ENTER) {
            if (!queue_uart_input('\r')) beep();
        } else if (key >= 0 && key <= UINT8_MAX) {
            uint8_t byte = key == '\n' ? '\r' : (uint8_t)key;
            if (!queue_uart_input(byte)) {
                beep();
            }
        }
        debugger.force_redraw = true;
        return true;
    }

    switch (key) {
        case 'q': case 'Q':
            debugger.quit = true;
            break;
        case ' ': case 'g': case 'G':
            if (vm->state == STEPPING && vm->steps == 0) {
                vm->state = RUNNING;
            } else {
                vm->state = STEPPING;
                vm->steps = 0;
            }
            break;
        case 's': case 'S':
            vm->state = STEPPING;
            vm->steps = 1;
            break;
        case 'n': case 'N':
            vm->state = STEPPING;
            vm->steps = 100;
            break;
        case 'r': case 'R':
            vm->state = RESET;
            break;
        case '\t':
            debugger.active_page = (debugger.active_page + 1u) % 4u;
            break;
        case 'c': case 'C':
            clear_active_page();
            break;
        case KEY_F(2):
            debugger.uart_focus = true;
#if MIPSEL_EMU_ENABLE_CONSOLE
            debugger.monitor_focus = false;
#endif
            break;
#if MIPSEL_EMU_ENABLE_CONSOLE
        case KEY_F(3): case ':':
            if (monitor_target_halted(vm) && monitor_window_usable()) {
                debugger.uart_focus = false;
                debugger.monitor_focus = true;
                debugger.monitor_scroll = 0;
                mipsel_console_cancel_input(&debugger.monitor);
            } else {
                beep();
            }
            break;
#endif
        case KEY_UP:
            if (debugger.trace_scroll + 1u < debugger.trace_count)
                ++debugger.trace_scroll;
            break;
        case KEY_DOWN:
            if (debugger.trace_scroll) --debugger.trace_scroll;
            break;
        case KEY_RESIZE:
            break;
        default:
            return true;
    }
    debugger.force_redraw = true;
    return true;
}
#endif

void debugger_poll(Registers *state, vmstate_t *vm, bool wait_for_input) {
    if (!debugger.initialized) return;
    if (signal_quit) debugger.quit = true;
    flush_uart_input();

    if (!debugger.tui) {
        poll_headless_uart_input();
        return;
    }

#ifdef MIPSEL_EMU_HAVE_CURSES
    if (debugger.tui) {
        uint64_t now;
        uint64_t interval = UINT64_C(1000000000) / debugger.refresh_hz;
        int key;

#if MIPSEL_EMU_ENABLE_CONSOLE
        if (debugger.monitor_focus && !monitor_target_halted(vm))
            mipsel_console_cancel_input(&debugger.monitor);
#endif

        timeout(wait_for_input ? 100 : 0);
        key = getch();
        if (key != ERR) {
            bool accepted = handle_key(key, state, vm);
            timeout(0);
            while (accepted && (key = getch()) != ERR)
                accepted = handle_key(key, state, vm);
        }

        now = monotonic_ns();
        if (debugger.force_redraw || now - debugger.last_draw_ns >= interval) {
            draw_full_tui(state, vm);
            debugger.last_draw_ns = now;
            debugger.force_redraw = false;
        }
    }
#else
    (void)state;
    (void)vm;
    (void)wait_for_input;
#endif
}

bool debugger_quit_requested(void) {
    return debugger.quit || signal_quit;
}

bool debugger_tui_enabled(void) {
    return debugger.tui;
}
