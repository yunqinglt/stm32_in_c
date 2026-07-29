#include "registers.h"

extern vmstate_pointer *status;

void startup(Registers *state) {
    while (1) {
        if (status->state == RUNNING) {/* Do something */}
        elif (status->state == STEPPING) {
            if (status->userdata != 0) status->userdata -= 1;
            else continue;
        } else continue;

    }
}