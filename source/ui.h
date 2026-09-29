// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_UI_H
#define BB_UI_H
#include "platform.h"
void ui_title(Game *,int,unsigned);
void ui_setup(Game *,const char *,const char *,const char *,int);
void ui_transition(Game *,int,unsigned,int,float);
void ui_pause(Game *,int);
void ui_graphics(Game *,int);
void ui_over(Game *);
void ui_error(Game *,const char *);
#endif
