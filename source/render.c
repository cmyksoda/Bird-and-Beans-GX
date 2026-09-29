// SPDX-License-Identifier: GPL-3.0-only
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static uint16_t background[WIDTH*HEIGHT];
static unsigned char background_layer[WIDTH*HEIGHT];
static uint16_t *canvas;static int canvas_w=WIDTH,canvas_h=HEIGHT;
void game_canvas(Game *g,int menu){g->menu_frame=menu;canvas=menu?g->menu_pixels:g->pixels;canvas_w=menu?640:WIDTH;canvas_h=menu?480:HEIGHT;}
static unsigned draw_layer;static int draw_blend;
static uint16_t rgb565(unsigned c){return ((c&31)<<11)|(((c>>5)&31)<<6)|((c>>10)&31);}
static uint16_t color(Game *g,int pal,int i){
    if(pal<0)return rgb565(vm_read(&g->vm,0x02320000+(pal==-2?512:0)+i*2,2));
    Blob b=g->blobs[pal];unsigned off=40+i*2;if(off+2>b.size)return 0;
    return rgb565(le16(b.p+off));
}
static uint16_t blend(uint16_t a,uint16_t b,int ea,int eb){
    if(ea<0)ea=0;if(ea>16)ea=16;if(eb<0)eb=0;if(eb>16)eb=16;
    int r=(((a>>11)*ea)+((b>>11)*eb))/16;
    int g=((((a>>6)&31)*ea)+(((b>>6)&31)*eb))/16;
    int bl=(((a&31)*ea)+((b&31)*eb))/16;
    if(r>31)r=31;if(g>31)g=31;if(bl>31)bl=31;return r<<11|g<<6|bl;
}
static void pixel(Game *g,int x,int y,uint16_t c){
    if((unsigned)x>=canvas_w||(unsigned)y>=canvas_h)return;int at=y*canvas_w+x;
    if(draw_layer){
        int first=draw_blend||(g->blend_first&draw_layer);
        if(first&&(g->blend_second&background_layer[at]))c=blend(c,background[at],g->blend_a,g->blend_b);
        if(draw_layer!=16){background[at]=c;background_layer[at]=draw_layer;}
    }
    (canvas?canvas:g->pixels)[at]=c;
}
static void tile(Game *g,int gfx,int pal,int index,int x,int y,int pb,int fx,int fy,int transparent){
    Blob b=g->blobs[gfx];unsigned at=48+index*32;if(at+32>b.size)return;
    for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){
        unsigned v=(b.p[at+yy*4+xx/2]>>((xx&1)*4))&15;
        if(v||!transparent)pixel(g,x+(fx?7-xx:xx),y+(fy?7-yy:yy),color(g,pal,pb*16+v));
    }
}
static void cell_render(Game *g,int nc,int cg,int pal,unsigned cell,float x,float y,float sx,float sy,unsigned angle,int flipx,int flipy,int pal_override,int priority){
    static const int wh[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
    Blob b=g->blobs[nc],gfx=g->blobs[cg];if(b.size<36||cell>=le16(b.p+24)||sx==0||sy==0)return;
    unsigned n=le16(b.p+24),size=le16(b.p+26)?16:8,base=24+le32(b.p+28);
    const uint8_t *c=b.p+base+cell*size;unsigned count=le16(c),oam=base+n*size+le32(c+4),shift=le32(b.p+32);if(shift>3)shift=0;
    float radians=(angle&65535)*(6.28318530718f/65536),cs=cosf(radians),sn=sinf(radians);
    if(flipx)sx=-sx;if(flipy)sy=-sy;
    for(int i=(int)count-1;i>=0;i--){
        if(oam+i*6+6>b.size)continue;const uint8_t *o=b.p+oam+i*6;
        unsigned a0=le16(o),a1=le16(o+2),a2=le16(o+4),shape=a0>>14;
        if(shape>2||((a0&0x300)==0x200))continue;
        if(priority>=0&&((a2>>10)&3)!=(unsigned)priority)continue;
        int w=wh[shape][a1>>14][0],h=wh[shape][a1>>14][1];
        int ox=((a1&511)^256)-256,oy=((a0&255)^128)-128;
        int fx=!(a0&256)&&!!(a1&4096),fy=!(a0&256)&&!!(a1&8192),pb=pal_override<0?a2>>12:pal_override;
        if(!angle&&sx==sy&&(sx==1||sx==-1||sx==2||sx==3)){
            if(sx<0){ox=-ox-w;fx=!fx;}if(sy<0){oy=-oy-h;fy=!fy;}
            int scale=(int)fabsf(sx);int bx=(int)floorf(x)+ox*scale,by=(int)floorf(y)+oy*scale;
            for(int ty=0;ty<h/8;ty++)for(int tx=0;tx<w/8;tx++){
                int idx=((a2&1023)<<shift)+ty*(w/8)+tx;
                int dx=bx+(fx?w/8-1-tx:tx)*8*scale,dy=by+(fy?h/8-1-ty:ty)*8*scale;
                if(scale==1)tile(g,cg,pal,idx,dx,dy,pb,fx,fy,1);
                else {unsigned at=48+idx*32;if(at+32>gfx.size)continue;
                    for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){
                        unsigned v=(gfx.p[at+yy*4+xx/2]>>((xx&1)*4))&15;if(!v)continue;uint16_t c=color(g,pal,pb*16+v);
                        int px=dx+(fx?7-xx:xx)*scale,py=dy+(fy?7-yy:yy)*scale;
                        for(int j=0;j<scale;j++)for(int k=0;k<scale;k++)pixel(g,px+k,py+j,c);
                    }
                }
            }
            continue;
        }
        float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
        for(int j=0;j<4;j++){float px=(ox+(j&1?w:0))*sx,py=(oy+(j&2?h:0))*sy;float xx=x+cs*px-sn*py,yy=y+sn*px+cs*py;if(xx<minx)minx=xx;if(xx>maxx)maxx=xx;if(yy<miny)miny=yy;if(yy>maxy)maxy=yy;}
        int l=fmaxf(0,floorf(minx)),r=fminf(canvas_w,ceilf(maxx)),t=fmaxf(0,floorf(miny)),bottom=fminf(canvas_h,ceilf(maxy));
        for(int yy=t;yy<bottom;yy++)for(int xx=l;xx<r;xx++){
            float dx=xx+0.5f-x,dy=yy+0.5f-y;
            int tx=(int)floorf((cs*dx+sn*dy)/sx-ox),ty=(int)floorf((-sn*dx+cs*dy)/sy-oy);
            if((unsigned)tx>=w||(unsigned)ty>=h)continue;if(fx)tx=w-1-tx;if(fy)ty=h-1-ty;
            unsigned at=48+(((a2&1023)<<shift)+(ty/8)*(w/8)+tx/8)*32+(ty&7)*4+(tx&7)/2;
            if(at>=gfx.size)continue;unsigned v=(gfx.p[at]>>((tx&1)*4))&15;
            if(v)pixel(g,xx,yy,color(g,pal,pb*16+v));
        }
    }
}
void game_cell(Game *g,int bank,int cell,int x,int y,int flip,int priority){
    static const int cells[]={31,30,29,34},chars[]={15,14,13,19};
    if(bank<0||bank>3)return;(void)priority;
    cell_render(g,cells[bank],chars[bank],bank==3?11:-1,cell,x,y,1,1,0,flip,0,-1,-1);
}
void game_art(Game *g,int nc,int cg,int pal,int cell,float x,float y,float scale){
    draw_layer=0;draw_blend=0;cell_render(g,nc,cg,pal,cell,x,y,scale,scale,0,0,0,-1,-1);
}
void game_background(Game *g,int ns,int cg,int pal){
    draw_layer=0;draw_blend=0;Blob b=g->blobs[ns];
    for(int y=0;y<(canvas_h+7)/8;y++)for(int x=0;x<(canvas_w+7)/8;x++){unsigned at=36+((y%24)*32+(x%32))*2;if(at+2>b.size)continue;unsigned v=le16(b.p+at);tile(g,cg,pal,v&1023,x*8,y*8,v>>12,v&1024,v&2048,0);}
}
int game_animation_cell(Game *g,int anim,int seq,unsigned ticks){
    Blob b=g->blobs[anim];if(b.size<40||(unsigned)seq>=le16(b.p+24))return 0;
    const uint8_t *s=b.p+24+le32(b.p+28)+seq*16;unsigned n=le16(s),frames=24+le32(b.p+32)+le32(s+12),duration=0;
    for(unsigned i=0;i<n;i++)duration+=le16(b.p+frames+i*8+4);if(!duration)return 0;ticks%=duration;
    unsigned fr=0;for(;fr+1<n;fr++){unsigned d=le16(b.p+frames+fr*8+4);if(ticks<d)break;ticks-=d;}
    unsigned data=24+le32(b.p+36)+le32(b.p+frames+fr*8);return data+2<=b.size?le16(b.p+data):0;
}
static void sprite(Game *g,uint32_t p,int priority){
    VM *m=&g->vm;if(!vm_read(m,p+0x4c,1))return;
    unsigned bank=vm_read(m,p+0xa8,4);if(bank>3||!(g->bank_screen[bank]&vm_read(m,p+0xbc,4)&0x1000))return;
    static const int anms[]={37,36,35,40},cells[]={31,30,29,34},chars[]={15,14,13,19};Blob b=g->blobs[anms[bank]];
    unsigned seq=vm_read(m,p+0xac,2);if(seq>=le16(b.p+24))return;
    const uint8_t *s=b.p+24+le32(b.p+28)+seq*16;unsigned count=le16(s),fr=vm_read(m,p+0xd8,4);
    if(!count)return;if(fr>=count)fr=count-1;
    unsigned at=24+le32(b.p+32)+le32(s+12)+fr*8;if(at+8>b.size)return;
    unsigned data=24+le32(b.p+36)+le32(b.p+at);if(data+2>b.size)return;
    float x=(int32_t)vm_read(m,p+0x44,4)/4096.f,y=(int32_t)vm_read(m,p+0x48,4)/4096.f+(int32_t)vm_read(m,p+0xb4,4);
    int override=(int32_t)vm_read(m,p+0xc0,4);if(override>=0&&override!=priority)return;
    draw_layer=16;draw_blend=vm_read(m,p+0xd0,4)==1;
    cell_render(g,cells[bank],chars[bank],-1,le16(b.p+data),x,y,(int32_t)vm_read(m,p+0x38,4)/4096.f,(int32_t)vm_read(m,p+0x3c,4)/4096.f,vm_read(m,p+0x40,2),vm_read(m,p+0xd4,1),vm_read(m,p+0xd5,1),(int32_t)vm_read(m,p+0xc4,4),override>=0?-1:priority);
}
static void map(Game *g,int l){
    draw_layer=1u<<l;draw_blend=0;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++){
        unsigned v=vm_read(&g->vm,0x02310000+l*4096+(y*32+x)*2,2);
        int sy=(y*8-g->bg_scroll[l])&255;
        if(sy>=192&&sy<249)continue;if(sy>=249)sy-=256;
        tile(g,g->bg_graphics[l],-2,v&1023,x*8,sy,v>>12,v&1024,v&2048,1);
    }
}
void game_render(Game *g){
    game_canvas(g,0);
    unsigned enabled=vm_read(&g->vm,vm_read(&g->vm,0x0204cbe0,4)+0x108,4)>>8;
    for(int y=0;y<HEIGHT;y++){
        uint16_t sky=g->gradient&&vm_read(&g->vm,0x0204cbf0,4)&0x2f00?rgb565(vm_read(&g->vm,0x0204cc00+y*2,2)):color(g,-2,0);
        for(int x=0;x<WIDTH;x++){int i=y*WIDTH+x;background[i]=g->pixels[i]=sky;background_layer[i]=32;}
    }
    for(int pr=3;pr>=0;pr--){
        for(int l=3;l>=0;l--)if((enabled&(1u<<l))&&g->bg_priority[l]==pr)map(g,l);
        if(enabled&16)for(int order=3;order>=0;order--)for(unsigned i=0;i<g->sprite_count;i++){
            uint32_t p=g->sprites[i];if(vm_read(&g->vm,p+0xae,1)==(unsigned)order)sprite(g,p,pr);
        }
    }
    if(g->brightness){int b=g->brightness;if(b>16)b=16;if(b< -16)b=-16;for(int i=0;i<WIDTH*HEIGHT;i++)g->pixels[i]=blend(g->pixels[i],b>0?0xffdf:0,16-abs(b),abs(b));}
    // Remove the outer mat without moving or trimming the rounded white frame.
    for(int y=0;y<HEIGHT;y++){
        int edge=y<3||y>=189?128:y==3||y==188?5:y==4||y==187?4:3;
        memset(g->pixels+y*WIDTH,0,edge*sizeof(uint16_t));
        memset(g->pixels+y*WIDTH+WIDTH-edge,0,edge*sizeof(uint16_t));
    }
    draw_layer=0;draw_blend=0;
}

// Original, compact 5x7 UI font. The game sprites and playfield come from the ROM.
void text_draw_scaled(Game *g,int x,int y,const char *s,uint16_t c,int scale){
    static const unsigned char digits[][5]={{62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},{39,69,69,69,57},{62,73,73,73,50},{1,113,9,5,3},{54,73,73,73,54},{38,73,73,73,62}};
    static const unsigned char letters[][5]={{126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},{3,4,120,4,3},{97,81,73,69,67}};
    for(;*s;s++,x+=6*scale){const unsigned char *p=NULL;int ch=*s;if(ch>='a'&&ch<='z')ch-=32;
        if(ch>='0'&&ch<='9')p=digits[ch-'0'];if(ch>='A'&&ch<='Z')p=letters[ch-'A'];
        if(p)for(int xx=0;xx<5;xx++)for(int yy=0;yy<7;yy++)if(p[xx]&(1<<yy))for(int a=0;a<scale;a++)for(int b=0;b<scale;b++)pixel(g,x+xx*scale+a,y+yy*scale+b,c);
        if(ch=='-')for(int xx=0;xx<5*scale;xx++)for(int a=0;a<scale;a++)pixel(g,x+xx,y+3*scale+a,c);
        if(ch==':')for(int a=0;a<scale;a++)for(int b=0;b<scale;b++){pixel(g,x+2*scale+a,y+2*scale+b,c);pixel(g,x+2*scale+a,y+5*scale+b,c);}
        if(ch=='.')for(int a=0;a<scale;a++)for(int b=0;b<scale;b++)pixel(g,x+2*scale+a,y+6*scale+b,c);
        if(ch=='/')for(int yy=0;yy<7;yy++)for(int a=0;a<scale;a++)for(int b=0;b<scale;b++)pixel(g,x+(4-yy*4/6)*scale+a,y+yy*scale+b,c);
    }
}

void text_draw(Game *g,int x,int y,const char *s,uint16_t c){text_draw_scaled(g,x,y,s,c,1);}
