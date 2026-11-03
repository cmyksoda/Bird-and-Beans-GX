/* SPDX-FileCopyrightText: 2026 quatric */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef INPUT_WII_H
#define INPUT_WII_H

#include <gccore.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- GameCube controller adapter for Wii U (HAVE_GCADAPTER) ----
 * Nintendo's WUP-028 and the third-party adapters that copy it, in either
 * mode (Wii U / PC). It is looked for every two seconds, so it can be
 * plugged in at any time; its four ports read as GameCube pads through
 * extrainput_compat.h. Sticks centre on where they rest
 * when a controller is plugged in; X+Y+Start held for three seconds takes
 * the centre again, as on the console. */
typedef struct {
    u16  buttons;                  /* PAD_BUTTON_* / PAD_TRIGGER_* */
    s8   stick_x, stick_y;         /* from the centre, y up, about -100..100 */
    s8   substick_x, substick_y;
    u8   trigger_l, trigger_r;     /* 0..255 */
    bool wireless;                 /* a WaveBird receiver: no rumble */
} input_gc_pad;

/* True while the adapter is plugged in and reporting. */
bool INPUT_GCAdapterConnected(void);
/* Port 0-3 as of the last INPUT_Update(); false when nothing is plugged
 * into it. */
bool INPUT_GCAdapterPad(int port, input_gc_pad *out);
/* Rumble needs the adapter's power (grey) cable plugged in. */
void INPUT_GCAdapterRumble(int port, bool on);
/* Where the adapter's steps and errors go, e.g. INPUT_GCAdapterSetLog(printf). */
void INPUT_GCAdapterSetLog(int (*print)(const char *fmt, ...));

#ifdef __cplusplus
}
#endif

#endif /* INPUT_WII_H */
