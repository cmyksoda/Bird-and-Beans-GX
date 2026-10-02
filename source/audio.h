// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_AUDIO_H
#define BB_AUDIO_H
#include "game.h"
#define AUDIO_RATE 32000
#define AUDIO_PLAYERS 16
#define AUDIO_UI_PLAYER 15
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void *engine;
    int paused;
    uint64_t mixed_frames, ui_end_frame;
} Audio;

int audio_init(Audio *, Blob);
int audio_load_voices(Audio *, const char *, uint32_t);
int audio_start_voice(Audio *);
void audio_free(Audio *);
void audio_stop(Audio *);
void audio_play(Audio *, int, int);
void audio_commands(Audio *, Game *);
void audio_mix(Audio *, int16_t *, unsigned);
#ifdef __cplusplus
}
#endif
#endif
