#ifndef MIPSEL_EMU_CONSOLE_H
#define MIPSEL_EMU_CONSOLE_H

#include "config.h"
#include "registers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transport-neutral monitor callbacks.  They are invoked synchronously from
 * mipsel_console_execute()/mipsel_console_feed(); callers must serialize those
 * calls with CPU execution and must consume or copy all emitted bytes before
 * the output callback returns.
 */
typedef bool (*mipsel_console_halted_fn)(void *opaque);
typedef bool (*mipsel_console_bus_read_fn)(void *opaque, uint32_t pa,
                                           unsigned width, uint32_t *value);
typedef bool (*mipsel_console_bus_write_fn)(void *opaque, uint32_t pa,
                                            unsigned width, uint32_t value);
typedef void (*mipsel_console_output_fn)(void *opaque, const char *bytes,
                                         size_t length);

enum {
    /* Echo printable input/backspace for a terminal-style CDC transport. */
    MIPSEL_CONSOLE_FLAG_ECHO = 1u << 0,
    /* Emit "mipsel-emu> " after each completed input line. */
    MIPSEL_CONSOLE_FLAG_PROMPT = 1u << 1,
};

typedef struct {
    Registers *registers;
    mipsel_console_halted_fn halted;
    mipsel_console_bus_read_fn bus_read;
    mipsel_console_bus_write_fn bus_write;
    void *target_opaque;
    mipsel_console_output_fn output;
    void *output_opaque;
    uint32_t flags;
} mipsel_console_config_t;

typedef enum {
    MIPSEL_CONSOLE_NO_COMMAND = 0,
    MIPSEL_CONSOLE_OK,
    MIPSEL_CONSOLE_STATE_CHANGED,
    MIPSEL_CONSOLE_ERROR,
    MIPSEL_CONSOLE_TARGET_NOT_HALTED,
} mipsel_console_result_t;

typedef struct {
    mipsel_console_config_t config;
    char input[MIPSEL_EMU_CONSOLE_LINE_SIZE];
    size_t input_length;
    size_t discarded_input;
    bool previous_was_cr;
    bool initialized;
} mipsel_console_t;

bool mipsel_console_init(mipsel_console_t *console,
                         const mipsel_console_config_t *config);
void mipsel_console_reset(mipsel_console_t *console);
void mipsel_console_cancel_input(mipsel_console_t *console);

/* Execute exactly one non-NUL-containing command line. */
mipsel_console_result_t mipsel_console_execute(
    mipsel_console_t *console, const char *line, size_t length);

/*
 * Feed a CDC/TUI byte stream through the fixed-size line editor.  CR, LF, and
 * CRLF terminate a command; Backspace/Delete and Ctrl-U edit it.  The return
 * value aggregates all completed commands in this span, preserving mutation,
 * error, and not-halted results instead of letting a trailing blank line hide
 * them.  It is NO_COMMAND when no line terminator completed a command.
 */
mipsel_console_result_t mipsel_console_feed(
    mipsel_console_t *console, const uint8_t *bytes, size_t length);

void mipsel_console_prompt(mipsel_console_t *console);
const char *mipsel_console_input(const mipsel_console_t *console);
size_t mipsel_console_input_length(const mipsel_console_t *console);

#ifdef __cplusplus
}
#endif

#endif
