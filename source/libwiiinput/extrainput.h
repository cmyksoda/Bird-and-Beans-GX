/* SPDX-FileCopyrightText: 2026 quatric */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _EXTRAINPUT_H_
#define _EXTRAINPUT_H_

/*
 * The GameCube controller adapter for Wii U, folded onto the PAD_* calls an app already makes: its four
 * ports read as GameCube pads. extrainput_compat.h wraps PAD_* so that including it (the Makefile passes
 * it with -include) is all an app needs.
 */

#include <gccore.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ExtraInput_Init(void);
void ExtraInput_Shutdown(void);

// Once per frame, from PAD_ScanPads()
void ExtraInput_Update(void);

// GameCube adapter port 0-3 as a GameCube pad (PAD_BUTTON_* bits, PAD_StickX-style values); false if nothing is plugged in
bool ExtraInput_GcPad(int port, u16 *held, u16 *down, u16 *up, s8 *stickX, s8 *stickY, s8 *cstickX, s8 *cstickY, u8 *triggerL, u8 *triggerR);

#ifdef __cplusplus
}
#endif

#endif
