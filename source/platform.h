// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H
#include "audio.h"
#include "display.h"
#define KEY_ACTION 1
#define KEY_RIGHT 16
#define KEY_LEFT 32
#define KEY_PAUSE 256
#define KEY_MENU 512
#define KEY_QUIT 1024
#define KEY_UP 4096
#define KEY_DOWN 8192
#define KEY_BACK 16384
#define KEY_CONTROLLER_CHANGED 32768
int platform_init(void);
unsigned platform_input(void);
int platform_connected(void);
double platform_seconds(void);
void platform_present(Game *);
void platform_audio(Audio *);
void platform_audio_reset(Audio *);
uint64_t platform_audio_played(void);
void platform_aspect(void);
int platform_wide(void);
int platform_pillarbox(void);
void platform_toggle_240p(void);
int platform_240p(void);
void platform_scale(void);
int platform_pixel_scale(void);
void platform_close(void);
void platform_capture(const char *,Game *);
#endif
