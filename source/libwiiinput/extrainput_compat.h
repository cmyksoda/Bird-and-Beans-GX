/* SPDX-FileCopyrightText: 2026 quatric */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _EXTRAINPUT_COMPAT_H_
#define _EXTRAINPUT_COMPAT_H_

/*
 * Passed to every file with -include (see the Makefile): makes the app's own PAD_* calls see the
 * GameCube controller adapter for Wii U as well. Its ports carry through PAD_ScanPads(), PAD_Buttons*(),
 * PAD_Stick*() and PAD_Trigger*(). Nothing in the app's own input code changes.
 */

#ifdef HW_RVL

#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "extrainput.h"

static inline void ExtraInput_PADInit(void)
{
	PAD_Init();
	ExtraInput_Init();
}

static inline s32 ExtraInput_WPADShutdown(void)
{
	ExtraInput_Shutdown();
	return WPAD_Shutdown();
}

#define PAD_Init()                   ExtraInput_PADInit()
#define WPAD_Shutdown()              ExtraInput_WPADShutdown()

// The GameCube controller adapter for Wii U: a real pad on a port wins, otherwise the adapter's pad is that port's pad
static inline u32 ExtraInput_PADScanPads(void)
{
	u32 mask = PAD_ScanPads();
	ExtraInput_Update();
	for (int i = 0; i < 4; i++)
	{
		if (ExtraInput_GcPad(i, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
		{
			mask |= 1u << i;
		}
	}
	return mask;
}

static inline u16 ExtraInput_PADButtons(int port, u16 real, int which)
{
	u16 held, down, up;
	if (!ExtraInput_GcPad(port, &held, &down, &up, NULL, NULL, NULL, NULL, NULL, NULL))
	{
		return real;
	}
	return real | (which == 0 ? held : which == 1 ? down : up);
}

static inline s8 ExtraInput_PADAxis(int port, s8 real, int which)
{
	s8 v[4];
	if (!ExtraInput_GcPad(port, NULL, NULL, NULL, &v[0], &v[1], &v[2], &v[3], NULL, NULL))
	{
		return real;
	}
	return abs(v[which]) > abs(real) ? v[which] : real;
}

static inline u8 ExtraInput_PADTrigger(int port, u8 real, int right)
{
	u8 l, r;
	if (!ExtraInput_GcPad(port, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &l, &r))
	{
		return real;
	}
	u8 v = right ? r : l;
	return v > real ? v : real;
}

#define PAD_ScanPads()               ExtraInput_PADScanPads()
#define PAD_ButtonsHeld(c)           ExtraInput_PADButtons((c), PAD_ButtonsHeld(c), 0)
#define PAD_ButtonsDown(c)           ExtraInput_PADButtons((c), PAD_ButtonsDown(c), 1)
#define PAD_ButtonsUp(c)             ExtraInput_PADButtons((c), PAD_ButtonsUp(c), 2)
#define PAD_StickX(c)                ExtraInput_PADAxis((c), PAD_StickX(c), 0)
#define PAD_StickY(c)                ExtraInput_PADAxis((c), PAD_StickY(c), 1)
#define PAD_SubStickX(c)             ExtraInput_PADAxis((c), PAD_SubStickX(c), 2)
#define PAD_SubStickY(c)             ExtraInput_PADAxis((c), PAD_SubStickY(c), 3)
#define PAD_TriggerL(c)              ExtraInput_PADTrigger((c), PAD_TriggerL(c), 0)
#define PAD_TriggerR(c)              ExtraInput_PADTrigger((c), PAD_TriggerR(c), 1)

#endif
#endif
