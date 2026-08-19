#ifndef MIPSEL_EMU_DEBUGGER_H
#define MIPSEL_EMU_DEBUGGER_H

#include "exception.h"
#include "registers.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enable_tui;
    bool start_paused;
    const char *trace_path;
    unsigned int refresh_hz;
} DebuggerConfig;

int debugger_init(const DebuggerConfig *config, Registers *state,
                  vmstate_t *vm);
void debugger_shutdown(void);

void debugger_instruction_begin(uint32_t pc, uint32_t pa, uint32_t word,
                                const Registers *state);
void debugger_instruction_end(const Registers *state);
void debugger_exception(const Registers *state, uint32_t exc_info,
                        uint8_t exc_code, VectorClass vector_class);
void debugger_uart_tx(uint8_t byte);
void debugger_board_reset(const Registers *state);

void debugger_poll(Registers *state, vmstate_t *vm, bool wait_for_input);
bool debugger_quit_requested(void);
bool debugger_tui_enabled(void);

#endif
