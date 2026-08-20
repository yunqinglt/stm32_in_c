#include "observer.h"

static mipsel_emu_observer_t active_observer;

void mipsel_emu_observer_set(const mipsel_emu_observer_t *observer) {
    if (observer) {
        active_observer = *observer;
    } else {
        active_observer = (mipsel_emu_observer_t){0};
    }
}

void mipsel_emu_observer_instruction_begin(uint32_t pc, uint32_t pa,
                                           uint32_t word,
                                           const Registers *state) {
    if (active_observer.instruction_begin) {
        active_observer.instruction_begin(active_observer.opaque, pc, pa,
                                          word, state);
    }
}

void mipsel_emu_observer_instruction_end(const Registers *state) {
    if (active_observer.instruction_end) {
        active_observer.instruction_end(active_observer.opaque, state);
    }
}

void mipsel_emu_observer_exception(const Registers *state,
                                   uint32_t exc_info, uint8_t exc_code,
                                   VectorClass vector_class) {
    if (active_observer.exception) {
        active_observer.exception(active_observer.opaque, state, exc_info,
                                  exc_code, vector_class);
    }
}
