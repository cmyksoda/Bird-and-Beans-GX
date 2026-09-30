// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "title_logo.h"
#define INK 0x2104
#define PAPER 0xff9c
#define GOLD 0xfe46
#define MUTED 0x6b2a
static void rect(Game *g,int x,int y,int w,int h,uint16_t c){int cw=g->menu_frame?640:256,ch=g->menu_frame?480:192;uint16_t *out=g->menu_frame?g->menu_pixels:g->pixels;for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)if((unsigned)i<cw&&(unsigned)j<ch)out[j*cw+i]=c;}
static void roundbox(Game *g,int x,int y,int w,int h,uint16_t c){rect(g,x+3,y,w-6,h,c);rect(g,x+1,y+1,w-2,h-2,c);rect(g,x,y+3,w,h-6,c);}
static void center(Game *g,int y,const char *s,uint16_t c){text_draw(g,(256-(int)strlen(s)*6+1)/2,y,s,c);}
static void arrow(Game *g,int x,int y,uint16_t c){for(int j=0;j<7;j++){int w=j<=3?j+1:7-j;rect(g,x,y+j,w,1,c);}}
static void dim(Game *g){for(int y=8;y<184;y++)for(int x=8;x<248;x++){int i=y*WIDTH+x;unsigned c=g->pixels[i];g->pixels[i]=((c>>11)*3/8)<<11|(((c>>5)&63)*3/8)<<5|(c&31)*3/8;}}
static void card(Game *g,int y,int h){roundbox(g,35,y+4,188,h,0x0841);roundbox(g,33,y,190,h,INK);roundbox(g,34,y+1,188,h-2,PAPER);rect(g,45,y+28,166,1,0xd653);}
// Game colours never set green's low bit, so this can't collide with art on the card layer.
#define CLEAR 0x0020
static uint16_t shade[WIDTH*HEIGHT];static int layered;
static void scale_into(uint16_t *out,const uint16_t *src,DisplayRect view,int keyed){
    // Match the final GX sample positions, including 240p's odd source rows.
    int sample_y=platform_240p()?0:1,source_x[640];
    for(int x=0;x<view.w;x++)source_x[x]=(2*x+1)*WIDTH/(2*view.w);
    for(int y=0;y<view.h;y++){
        int sy=(2*y+sample_y)*HEIGHT/(2*view.h);uint16_t *row=out+(view.y+y)*640+view.x;
        for(int x=0;x<view.w;x++){uint16_t c=src[sy*WIDTH+source_x[x]];if(!keyed||c!=CLEAR)row[x]=c;}
    }
}
// In Fill, cards go on a clear layer that ui_compose shows at the Fixed size over the stretched game.
static void overlay(Game *g){dim(g);layered=!platform_pixel_scale();if(!layered)return;memcpy(shade,g->pixels,sizeof(shade));for(int i=0;i<WIDTH*HEIGHT;i++)g->pixels[i]=CLEAR;}
void ui_compose(Game *g){
    if(!layered)return;layered=0;int wide=platform_wide(),box=platform_pillarbox();
    game_canvas(g,CANVAS_FULLSCREEN);memset(g->menu_pixels,0,sizeof(g->menu_pixels));
    scale_into(g->menu_pixels,shade,display_rect(wide,box,0,0),0);scale_into(g->menu_pixels,g->pixels,display_rect(wide,box,1,0),1);
}
static void title_center(Game *g,int y,const char *s,uint16_t c){text_draw_scaled(g,(640-(int)strlen(s)*12+2)/2,y,s,c,2);}
static void title_logo(Game *g){
    static unsigned char coverage[TITLE_LOGO_W*TITLE_LOGO_H];
    unsigned at=0;int x=(640-TITLE_LOGO_W)/2,y=(128-TITLE_LOGO_H)/2;
    for(unsigned i=0;i<sizeof(title_logo_rle);i+=2){
        for(unsigned j=0;j<title_logo_rle[i];j++)coverage[at++]=title_logo_rle[i+1];
    }
    for(int yy=0;yy<TITLE_LOGO_H;yy++)for(int xx=0;xx<TITLE_LOGO_W;xx++){
        unsigned outline=0,ink=coverage[yy*TITLE_LOGO_W+xx];
        for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){
            if(dx*dx+dy*dy>10||(unsigned)(xx+dx)>=TITLE_LOGO_W||(unsigned)(yy+dy)>=TITLE_LOGO_H)continue;
            unsigned a=coverage[(yy+dy)*TITLE_LOGO_W+xx+dx];if(a>outline)outline=a;
        }
        unsigned pos=(y+yy)*640+x+xx,c=g->menu_pixels[pos];
        unsigned r=(c>>11)*(63-outline)/63,gr=((c>>5)&63)*(63-outline)/63,b=(c&31)*(63-outline)/63;
        r=r*(63-ink)/63+31*ink/63;gr=gr*(63-ink)/63+ink;b=b*(63-ink)/63+31*ink/63;
        g->menu_pixels[pos]=r<<11|gr<<5|b;
    }
}
static void title_card(Game *g,int mode,int cell,int x){
    static uint16_t **cache;static unsigned count;
    if(!cache){count=le16(g->blobs[61].p+24);cache=calloc(count,sizeof(*cache));}
    if(!cache||(unsigned)cell>=count){game_art(g,61,50,41,cell,mode?334:306,308,2);return;}
    if(!cache[cell]){
        cache[cell]=malloc(108*96*sizeof(uint16_t));
        if(!cache[cell]){game_art(g,61,50,41,cell,mode?334:306,308,2);return;}
        game_canvas(g,0);for(int p=0;p<WIDTH*HEIGHT;p++)g->pixels[p]=0xf81f;
        game_art(g,61,50,41,cell,mode?-10:118,68,1);
        for(int y=0;y<96;y++)memcpy(cache[cell]+y*108,g->pixels+y*256,108*sizeof(uint16_t));
        game_canvas(g,CANVAS_FULLSCREEN);
    }
    for(int y=0;y<96;y++){
        uint16_t *out=g->menu_pixels+(172+y*2)*640+x;
        for(int xx=0;xx<108;xx++){uint16_t c=cache[cell][y*108+xx];if(c!=0xf81f)out[xx*2]=out[xx*2+1]=out[640+xx*2]=out[640+xx*2+1]=c;}
    }
}
void ui_setup(Game *g,const char *heading,const char *line1,const char *line2,int busy){
    game_canvas(g,CANVAS_FULLSCREEN);
    for(int y=0;y<480;y++)for(int x=0;x<640;x++)g->menu_pixels[y*640+x]=((x/8+y/8)&1)?0xe71c:0xef5d;
    title_logo(g);
    roundbox(g,42,171,556,224,INK);roundbox(g,44,173,552,220,PAPER);
    roundbox(g,64,193,512,42,GOLD);title_center(g,207,heading,INK);
    title_center(g,265,line1,INK);title_center(g,291,line2,INK);
    title_center(g,352,busy?"PLEASE WAIT":"HOME / Z TO EXIT",MUTED);
}
void ui_title(Game *g,int mode,unsigned tick){
    static uint16_t backdrop[640*480];static int ready;
    game_canvas(g,CANVAS_FULLSCREEN);
    if(!ready){game_background(g,54,45,42);title_logo(g);title_center(g,135,"CHOOSE A GAME",MUTED);memcpy(backdrop,g->menu_pixels,sizeof(backdrop));ready=1;}
    else memcpy(g->menu_pixels,backdrop,sizeof(backdrop));
    for(int i=0;i<2;i++){
        int x=i?354:70;
        if(mode==i)roundbox(g,x-6,166,228,204,GOLD);
        title_card(g,i,game_animation_cell(g,67,34+i,tick),x);
        rect(g,x+26,244,164,18,0xffff);
        char score[24];snprintf(score,sizeof(score),"BEST %06u",g->high[i]);text_draw_scaled(g,x+42,246,score,INK,2);
    }
    title_center(g,395,mode?"SPIT SEEDS AT FALLING BEANS":"CATCH BEANS WITH YOUR TONGUE",INK);
    roundbox(g,238,434,164,34,INK);roundbox(g,238,432,164,34,GOLD);
    title_center(g,442,"A / 2 PLAY",INK);
    text_draw_scaled(g,24,444,"LEFT / RIGHT",MUTED,2);
    text_draw_scaled(g,460,444,"HOME / Z EXIT",MUTED,2);
}
void ui_transition(Game *g,int mode,unsigned tick,int phase,float progress){
    static uint16_t mascot[640*480],backdrop[640*480];static int cached,last_phase=-1;
    if(!cached){
        game_canvas(g,1);memset(g->menu_pixels,0,sizeof(g->menu_pixels));
        game_art(g,34,19,11,game_animation_cell(g,40,0,0),368,286,2);
        memcpy(mascot,g->menu_pixels,sizeof(mascot));cached=1;
    }
    if(phase==1){last_phase=phase;game_canvas(g,CANVAS_FULLSCREEN);memcpy(g->menu_pixels,mascot,sizeof(mascot));return;}
    if(progress<0)progress=0;if(progress>1)progress=1;
    if(phase!=last_phase){
        if(phase==0){ui_title(g,mode,tick);memcpy(backdrop,g->menu_pixels,sizeof(backdrop));}
        else{
            game_render(g);
            DisplayRect view=display_rect(platform_wide(),platform_pillarbox(),platform_pixel_scale(),0);
            memset(backdrop,0,sizeof(backdrop));scale_into(backdrop,g->pixels,view,0);
        }
        last_phase=phase;
    }
    game_canvas(g,CANVAS_FULLSCREEN);
    unsigned weight=(unsigned)((phase==0?progress:1-progress)*32),inv=32-weight;
    for(int i=0;i<640*480;i++){
        unsigned a=backdrop[i],b=mascot[i];
        a=(a|a<<16)&0x07e0f81f;b=(b|b<<16)&0x07e0f81f;
        unsigned c=((a*inv+b*weight)>>5)&0x07e0f81f;
        g->menu_pixels[i]=c|c>>16;
    }
}
void ui_pause(Game *g,int selected){
    overlay(g);int rows=4,y=25,h=144;card(g,y,h);
    center(g,y+12,"PAUSED",INK);
    const char *labels[]={"CONTINUE","RESTART","GRAPHICS OPTIONS","RETURN TO TITLE"};
    for(int i=0;i<rows;i++){
        int yy=y+36+i*22;
        if(i==selected){roundbox(g,44,yy-5,168,18,GOLD);arrow(g,51,yy,INK);}
        text_draw(g,65,yy,labels[i],INK);
    }
    game_art(g,31,15,3,0,201,y+20,1);
    center(g,y+h-12,"A / 2 SELECT   B BACK",MUTED);
}
void ui_graphics(Game *g,int selected){
    overlay(g);int wide=platform_wide(),rows=wide?4:3,y=wide?25:36,h=wide?144:121;card(g,y,h);
    center(g,y+12,"GRAPHICS OPTIONS",INK);
    const char *scale=platform_pixel_scale()?"PIXEL SCALE: FIXED":"PIXEL SCALE: FILL";
    const char *labels[]={platform_240p()?"240P MODE: ON":"240P MODE: OFF",scale,wide?(platform_pillarbox()?"ASPECT RATIO: 4:3":"ASPECT RATIO: 16:9"):"BACK","BACK"};
    for(int i=0;i<rows;i++){
        int yy=y+36+i*22;
        if(i==selected){roundbox(g,44,yy-5,168,18,GOLD);arrow(g,51,yy,INK);}
        text_draw(g,65,yy,labels[i],INK);
    }
    center(g,y+h-12,"A / 2 CHANGE   B BACK",MUTED);
}
void ui_over(Game *g){
    overlay(g);card(g,49,94);center(g,61,"GAME OVER",INK);
    char s[32];snprintf(s,sizeof(s),"SCORE %06u",game_score(g));center(g,87,s,INK);
    snprintf(s,sizeof(s),"BEST  %06u",g->high[g->mode]);center(g,99,s,MUTED);
    roundbox(g,53,116,150,17,GOLD);center(g,121,"A / 2 PLAY AGAIN",INK);
    center(g,153,"HOME / Z RETURN TO TITLE",PAPER);
}
void ui_error(Game *g,const char *message){overlay(g);card(g,47,98);center(g,60,"SOMETHING WENT WRONG",INK);text_draw(g,44,88,message,INK);center(g,126,"HOME / Z RETURN TO TITLE",INK);}
