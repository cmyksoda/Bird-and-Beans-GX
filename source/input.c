// SPDX-License-Identifier: GPL-3.0-only
#include "input.h"

unsigned input_select(InputState *s, const unsigned held[INPUT_DEVICES], unsigned present) {
    int old = s->owner, next = -1;
    if (old >= 0 && !(present & (1u << old))) {
        s->owner = -1;
        s->connected = 0;
    }
    for (int i = 0; i < INPUT_DEVICES; i++) {
        unsigned value = present & (1u << i) ? held[i] : 0;
        unsigned pressed = value & ~s->previous[i];
        // A fresh button or stick direction takes over the whole controller.
        // A stick left held on an inactive controller cannot steal it back.
        if (value && i != s->owner && (pressed || s->owner < 0) && next < 0) next = i;
        s->previous[i] = value;
    }
    if (next >= 0) {
        s->owner = next;
        s->connected = 1;
    }
    if (s->owner < 0) return 0;
    return held[s->owner] | (s->owner != old ? KEY_CONTROLLER_CHANGED : 0);
}
