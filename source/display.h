// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_DISPLAY_H
#define BB_DISPLAY_H
typedef struct {int x,y,w,h;} DisplayRect;
// Coordinates use the 640x480 presentation canvas, including in 240p.
static inline DisplayRect display_rect(int wide,int pillarbox,int scale,int fullscreen){
    if(fullscreen)return (DisplayRect){0,0,640,480};
    int w=scale?512:640,h=scale?384:480;
    if(wide&&(scale||pillarbox))w=w*3/4;
    return (DisplayRect){(640-w)/2,(480-h)/2,w,h};
}
#endif
