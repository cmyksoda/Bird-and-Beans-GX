/* SPDX-FileCopyrightText: 2026 quatric */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBINPUT_INTERNAL_H
#define LIBINPUT_INTERNAL_H

/* Trimmed from libwiiinput: only what the GameCube adapter driver needs. */

#include <gctypes.h>
#include <stdbool.h>
#include "input_wii.h"

/* the GameCube controller adapter for Wii U, for the INPUT_GCAdapter* calls in libinput_gcadapter.c */
#ifdef HAVE_GCADAPTER
void inpw_gcadapter_init(bool enabled);
void inpw_gcadapter_shutdown(void);
void inpw_gcadapter_update(void);
#endif

#endif
