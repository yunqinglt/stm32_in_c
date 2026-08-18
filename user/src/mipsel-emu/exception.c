#include "exception.h"
#include <stdint.h>

// for startup reset initial
void reset_cpu(Registers *state) {

}

// Building
void raise_exception(Registers *state, uint32_t exc_info, uint8_t exc_code, VectorClass class) {

}