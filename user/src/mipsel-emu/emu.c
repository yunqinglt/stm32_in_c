#include "registers.h"

extern vmstate_pointer *status;

void startup(Registers *state) {
    while (1) {
        if (status->state == RUNNING) {/* Do something */}
        else if (status->state == STEPPING) {
            if (status->userdata.u_ud != 0) status->userdata.u_ud -= 1;
            else continue;
        } else continue;

    }
}