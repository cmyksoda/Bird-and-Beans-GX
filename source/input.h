// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_INPUT_H
#define BB_INPUT_H
#include "platform.h"
#define INPUT_DEVICES 8
typedef struct {
    unsigned previous[INPUT_DEVICES];
    int owner,connected;
} InputState;
#define INPUT_STATE_INIT {{0},-1,1}
unsigned input_select(InputState *,const unsigned held[INPUT_DEVICES],unsigned present);
#endif
