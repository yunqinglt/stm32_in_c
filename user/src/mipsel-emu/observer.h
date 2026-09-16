#ifndef MIPSEL_EMU_OBSERVER_H
#define MIPSEL_EMU_OBSERVER_H

#include "exception.h"
#include "registers.h"
#include <stdint.h>

/*
 * Optional instruction/exception instrumentation boundary.  The portable
 * core owns no terminal, file, or allocator state; a hosted debugger may
 * register these callbacks instead.  The current emulator models one board,
 * so observer registration is process-global just like the platform bus.
 */
typedef struct {
    void *opaque;
    void (*instruction_begin)(void *opaque, uint32_t pc, uint32_t pa,
                              uint32_t word, const Registers *state);
    void (*instruction_end)(void *opaque, const Registers *state);
    void (*exception)(void *opaque, const Registers *state,
                      uint32_t exc_info, uint8_t exc_code,
                      VectorClass vector_class);
} mipsel_emu_observer_t;

#if CONFIG_IS_EMBEDDED_SYSTEM
/* Instruction tracing is a hosted diagnostic feature.  Static no-op
 * implementations let the CPU hot path compile away without retaining an
 * observer object or callback calls in the firmware image. */
static inline void mipsel_emu_observer_set(
    const mipsel_emu_observer_t *observer) {
    (void)observer;
}

static inline void mipsel_emu_observer_instruction_begin(
    uint32_t pc, uint32_t pa, uint32_t word, const Registers *state) {
    (void)pc;
    (void)pa;
    (void)word;
    (void)state;
}

static inline void mipsel_emu_observer_instruction_end(
    const Registers *state) {
    (void)state;
}

static inline void mipsel_emu_observer_exception(
    const Registers *state, uint32_t exc_info, uint8_t exc_code,
    VectorClass vector_class) {
    (void)state;
    (void)exc_info;
    (void)exc_code;
    (void)vector_class;
}
#else
void mipsel_emu_observer_set(const mipsel_emu_observer_t *observer);
void mipsel_emu_observer_instruction_begin(uint32_t pc, uint32_t pa,
                                           uint32_t word,
                                           const Registers *state);
void mipsel_emu_observer_instruction_end(const Registers *state);
void mipsel_emu_observer_exception(const Registers *state,
                                   uint32_t exc_info, uint8_t exc_code,
                                   VectorClass vector_class);
#endif

#endif
