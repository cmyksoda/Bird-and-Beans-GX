// SPDX-License-Identifier: GPL-3.0-only
#include "platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture, *menu_texture;
static SDL_AudioDeviceID device;
static SDL_GameController *pad;
static int connected = 1, wide, pillarbox, low_resolution, pixel_scale;
static uint64_t audio_queued;
static double silent_audio_start;

int platform_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER)) return 0;
    window = SDL_CreateWindow("Bird & Beans Wii - compatibility preview", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, (getenv("BB_WIDE") ? 1024 : 768), 576,
                              SDL_WINDOW_RESIZABLE);
    if (!window) return 0;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) return 0;

    texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 256, 192);
    menu_texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 640, 480);

    SDL_AudioSpec spec = {0};
    spec.freq = AUDIO_RATE;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 1024;
    device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (device) SDL_PauseAudioDevice(device, 0);

    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            break;
        }
    return texture != NULL;
}

unsigned platform_input(void) {
    unsigned in = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) in |= KEY_QUIT;
        if (e.type == SDL_CONTROLLERDEVICEADDED && !pad)
            pad = SDL_GameControllerOpen(e.cdevice.which);
        if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad && !SDL_GameControllerGetAttached(pad)) {
            SDL_GameControllerClose(pad);
            pad = NULL;
            in |= KEY_PAUSE;
        }
    }

    const Uint8 *k = SDL_GetKeyboardState(NULL);
    if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) in |= KEY_LEFT;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) in |= KEY_RIGHT;
    if (k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_RETURN]) in |= KEY_ACTION;
    if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) in |= KEY_UP;
    if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) in |= KEY_DOWN;
    if (k[SDL_SCANCODE_BACKSPACE]) in |= KEY_BACK;
    if (k[SDL_SCANCODE_P]) in |= KEY_PAUSE;
    if (k[SDL_SCANCODE_ESCAPE]) in |= KEY_MENU;

    if (pad) {
        int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (y < -12000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
            in |= KEY_UP;
        if (y > 12000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            in |= KEY_DOWN;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) in |= KEY_BACK;
        int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        if (x < -8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            in |= KEY_LEFT;
        if (x > 8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            in |= KEY_RIGHT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)
            || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
            in |= KEY_ACTION;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) in |= KEY_PAUSE;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK)) in |= KEY_MENU;
    }

    return in;
}

int platform_connected(void) {
    return connected;
}

double platform_seconds(void) {
    return (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
}

void platform_present(Game *g) {
    SDL_Texture *current = g->menu_frame ? menu_texture : texture;
    SDL_UpdateTexture(current, NULL, g->menu_frame ? g->menu_pixels : g->pixels,
                      g->menu_frame ? 1280 : 512);

    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    DisplayRect view =
        display_rect(platform_wide(), pillarbox, pixel_scale, g->menu_frame == CANVAS_FULLSCREEN);
    SDL_Rect r = {view.x * w / 640, view.y * h / 480, view.w * w / 640, view.h * h / 480};

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, current, NULL, &r);
    SDL_RenderPresent(renderer);
    SDL_Delay(1);
}

void platform_audio(Audio *a) {
    int16_t b[2048];
    if (!device) {
        uint64_t target = (platform_seconds() - silent_audio_start) * AUDIO_RATE;
        while (a->mixed_frames < target)
            audio_mix(a, b, 1024);
        return;
    }
    while (SDL_GetQueuedAudioSize(device) < 4096) {
        audio_mix(a, b, 1024);
        SDL_QueueAudio(device, b, sizeof(b));
        audio_queued += 1024;
    }
}

void platform_audio_reset(Audio *a) {
    if (device) SDL_ClearQueuedAudio(device);
    audio_queued = 0;
    a->mixed_frames = 0;
    a->ui_end_frame = 0;
    silent_audio_start = platform_seconds();
}

uint64_t platform_audio_played(void) {
    if (!device) return (platform_seconds() - silent_audio_start) * AUDIO_RATE;
    uint64_t pending = SDL_GetQueuedAudioSize(device) / 4 + 1024;
    return audio_queued > pending ? audio_queued - pending : 0;
}

int platform_wide(void) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    wide = w * 2 > h * 3;
    if (!wide) pillarbox = 0;
    return wide;
}

int platform_pillarbox(void) {
    return platform_wide() && pillarbox;
}

void platform_aspect(void) {
    if (platform_wide()) pillarbox = !pillarbox;
}

void platform_toggle_240p(void) {
    low_resolution = !low_resolution;
}

int platform_240p(void) {
    return low_resolution;
}

void platform_scale(void) {
    pixel_scale = pixel_scale ? 0 : 2;
}

int platform_pixel_scale(void) {
    return pixel_scale;
}

void platform_capture(const char *p, Game *g) {
    FILE *f = fopen(p, "wb");
    if (!f) return;
    int w = g->menu_frame ? 640 : 256, h = g->menu_frame ? 480 : 192;
    const uint16_t *pixels = g->menu_frame ? g->menu_pixels : g->pixels;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned c = pixels[i];
        unsigned char b[3] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63,
                              (c & 31) * 255 / 31};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}

void platform_close(void) {
    if (device) SDL_CloseAudioDevice(device);
    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(menu_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
