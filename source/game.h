// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_GAME_H
#define BB_GAME_H
#include "vm.h"
#define SCENE 0x02100000u
#define STATE 0x02101000u
#define WIDTH 256
#define HEIGHT 192
#define CANVAS_FULLSCREEN 2
#define MAX_SPRITES 160
#define MAX_TASKS 64

typedef struct {
    uint8_t *p;
    uint32_t size;
} Blob;

typedef struct {
    uint32_t ptr;
    int active, done, started;
} Task;

typedef struct {
    VM vm;
    uint8_t *pack;
    size_t pack_size;
    Blob blobs[82];
    uint32_t sprites[MAX_SPRITES];
    unsigned sprite_count;
    Task tasks[MAX_TASKS];
    uint32_t heap, frame, previous, high[2];
    uint32_t free_blocks[256];
    uint16_t tilemap[4][1024];
    int bg_graphics[4], bg_priority[4], bg_scroll[4], palette, mode, dead;
    uint32_t bank_screen[4], gradient;
    unsigned blend_first, blend_second;
    int blend_a, blend_b, brightness;
    uint32_t audio_ticks[16];
    int audio_active[16], audio_sequence[16];
    int sound_queue[32], sound_channel[32], sound_count;
    uint16_t pixels[WIDTH * HEIGHT];
    uint16_t menu_pixels[640 * 480];
    int menu_frame;
} Game;

#ifdef __cplusplus
extern "C" {
#endif
uint16_t le16(const void *);
uint32_t le32(const void *);
int game_load(Game *, const char *);
int game_load_memory(Game *, const void *, size_t);
int game_start(Game *, int, uint32_t);
int game_tick(Game *, uint32_t);
void game_render(Game *);
void game_free(Game *);
void text_draw(Game *, int, int, const char *, uint16_t);
void text_draw_scaled(Game *, int, int, const char *, uint16_t, int);
void game_canvas(Game *, int);
unsigned game_score(Game *);
void game_art(Game *, int, int, int, int, float, float, float);
void game_background(Game *, int, int, int);
int game_animation_cell(Game *, int, int, unsigned);
void game_cell(Game *, int, int, int, int, int, int);
#ifdef __cplusplus
}
#endif
#endif
