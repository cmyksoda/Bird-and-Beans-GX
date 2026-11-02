/* SPDX-FileCopyrightText: 2026 quatric */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HW_RVL
#include <ogc/usb.h>
#include "extrainput.h"
#include "input_wii.h"
#include "libinput_internal.h"

#define GC_PORTS 4

static bool running = false;
static struct {
	bool present;
	u16 held, down, up;
	s8 sx, sy, csx, csy;
	u8 tl, tr;
} gc[GC_PORTS];

void ExtraInput_Init(void)
{
	if (running)
	{
		return;
	}
	USB_Initialize();
	inpw_gcadapter_init(true);
	running = true;
}

void ExtraInput_Shutdown(void)
{
	if (!running)
	{
		return;
	}
	running = false;
	inpw_gcadapter_shutdown();
	for (int i = 0; i < GC_PORTS; i++)
	{
		gc[i].present = false;
	}
}

void ExtraInput_Update(void)
{
	if (!running)
	{
		return;
	}

	inpw_gcadapter_update();

	for (int port = 0; port < GC_PORTS; port++)
	{
		input_gc_pad p;
		if (!INPUT_GCAdapterPad(port, &p))
		{
			gc[port].present = false;
			gc[port].held = gc[port].down = gc[port].up = 0;
			continue;
		}
		gc[port].present = true;
		gc[port].down = p.buttons & ~gc[port].held;
		gc[port].up = gc[port].held & ~p.buttons;
		gc[port].held = p.buttons;
		gc[port].sx = p.stick_x;
		gc[port].sy = p.stick_y;
		gc[port].csx = p.substick_x;
		gc[port].csy = p.substick_y;
		gc[port].tl = p.trigger_l;
		gc[port].tr = p.trigger_r;
	}
}

bool ExtraInput_GcPad(int port, u16 *held, u16 *down, u16 *up, s8 *stickX, s8 *stickY, s8 *cstickX, s8 *cstickY, u8 *triggerL, u8 *triggerR)
{
	if (port < 0 || port >= GC_PORTS || !gc[port].present)
	{
		return false;
	}
	if (held) *held = gc[port].held;
	if (down) *down = gc[port].down;
	if (up) *up = gc[port].up;
	if (stickX) *stickX = gc[port].sx;
	if (stickY) *stickY = gc[port].sy;
	if (cstickX) *cstickX = gc[port].csx;
	if (cstickY) *cstickY = gc[port].csy;
	if (triggerL) *triggerL = gc[port].tl;
	if (triggerR) *triggerR = gc[port].tr;
	return true;
}

#endif
