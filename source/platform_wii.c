// SPDX-License-Identifier: GPL-3.0-only
#include "platform.h"
#include "input.h"
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <asndlib.h>
#include <ogc/lwp_watchdog.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static GXRModeObj render_mode,normal_mode,*mode=&render_mode;static void *xfb[2],*fifo;static int front,wide,pillarbox,low_resolution,pixel_scale,video_visible;static volatile int quit;
static InputState controllers=INPUT_STATE_INIT;
static uint16_t texels[640*480] ATTRIBUTE_ALIGN(32);static int16_t audio_buffers[3][2048] ATTRIBUTE_ALIGN(32);static int audio_started;
static uint64_t audio_completed,audio_segment_frames;static int audio_pending=-1;
static void reset(u32 a,void*b){(void)a;(void)b;quit=1;}
static void power(void){quit=1;}
static void render_mode_setup(void){
    GX_SetViewport(0,0,mode->fbWidth,mode->efbHeight,0,1);GX_SetScissor(0,0,mode->fbWidth,mode->efbHeight);
    GX_SetDispCopyYScale((float)mode->xfbHeight/mode->efbHeight);GX_SetDispCopySrc(0,0,mode->fbWidth,mode->efbHeight);GX_SetDispCopyDst(mode->fbWidth,mode->xfbHeight);
    GX_SetCopyFilter(mode->aa,mode->sample_pattern,GX_FALSE,mode->vfilter);GX_SetFieldMode(mode->field_rendering,mode->viHeight==2*mode->xfbHeight?GX_ENABLE:GX_DISABLE);
}
void platform_toggle_240p(void){
    u32 tv=normal_mode.viTVMode>>2;
    GXRModeObj *low=tv==VI_MPAL?&TVMpal240Ds:(tv==VI_PAL||tv==VI_EURGB60)?&TVEurgb60Hz240Ds:&TVNtsc240Ds;
    GX_DrawDone();VIDEO_SetBlack(TRUE);VIDEO_Flush();VIDEO_WaitVSync();
    render_mode=low_resolution?normal_mode:*low;mode->viWidth=704;mode->viXOrigin=(720-704)/2;
    for(int i=0;i<2;i++)VIDEO_ClearFrameBuffer(mode,xfb[i],COLOR_BLACK);
    VIDEO_Configure(mode);VIDEO_SetNextFramebuffer(xfb[front]);VIDEO_Flush();VIDEO_WaitVSync();
    render_mode_setup();video_visible=0;low_resolution=!low_resolution;
    fprintf(stderr,"Video: %s tv=%u framebuffer=%ux%u\n",low_resolution?"240p":"normal",mode->viTVMode,mode->fbWidth,mode->xfbHeight);
}
int platform_init(void){
    SYS_STDIO_Report(true);VIDEO_Init();VIDEO_SetBlack(TRUE);VIDEO_Flush();VIDEO_WaitVSync();PAD_Init();WPAD_Init();for(int i=0;i<4;i++)WPAD_SetDataFormat(i,WPAD_FMT_BTNS);
    render_mode=*VIDEO_GetPreferredMode(NULL);mode->viWidth=704;mode->viXOrigin=(720-704)/2;normal_mode=render_mode;fprintf(stderr,"Video: %d %d %d %d %d\n",mode->fbWidth,mode->efbHeight,mode->xfbHeight,mode->viWidth,mode->viHeight);wide=CONF_GetAspectRatio()==CONF_ASPECT_16_9;
    for(int i=0;i<2;i++)xfb[i]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));fifo=memalign(32,256*1024);if(!xfb[0]||!xfb[1]||!fifo)return 0;
    for(int i=0;i<2;i++)VIDEO_ClearFrameBuffer(mode,xfb[i],COLOR_BLACK);
    VIDEO_Configure(mode);VIDEO_SetNextFramebuffer(xfb[0]);VIDEO_Flush();VIDEO_WaitVSync();
    GX_Init(fifo,256*1024);GX_SetCopyClear((GXColor){0,0,0,255},0xffffff);
    render_mode_setup();
    GX_SetCullMode(GX_CULL_NONE);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_TRUE);
    GX_SetNumChans(0);GX_SetNumTexGens(1);GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetNumTevStages(1);GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    Mtx m;guMtxIdentity(m);GX_LoadPosMtxImm(m,GX_PNMTX0);
    fatInitDefault();ASND_Init();ASND_Pause(0);SYS_SetResetCallback(reset);SYS_SetPowerCallback(power);return 1;
}
static unsigned axis(float x){return x < -0.25f?KEY_LEFT:x>0.25f?KEY_RIGHT:0;}
static unsigned remote(int i,int *ok){
    u32 type;if(WPAD_Probe(i,&type)!=WPAD_ERR_NONE){*ok=0;return 0;}*ok=1;WPADData*d=WPAD_Data(i);u32 b=WPAD_ButtonsHeld(i);unsigned r=0;
    if(b&(WPAD_BUTTON_A|WPAD_BUTTON_B|WPAD_BUTTON_1|WPAD_BUTTON_2))r|=KEY_ACTION;
    if(b&WPAD_BUTTON_B)r|=KEY_BACK;
    if(b&WPAD_BUTTON_PLUS)r|=KEY_PAUSE;if(b&WPAD_BUTTON_HOME)r|=KEY_MENU;
    if(type==WPAD_EXP_CLASSIC){
        if(b&WPAD_CLASSIC_BUTTON_UP)r|=KEY_UP;if(b&WPAD_CLASSIC_BUTTON_DOWN)r|=KEY_DOWN;if(b&WPAD_CLASSIC_BUTTON_B)r|=KEY_BACK;
        float cy=cosf(d->exp.classic.ljs.ang*0.0174532925f)*d->exp.classic.ljs.mag;if(cy>0.4f)r|=KEY_UP;if(cy< -0.4f)r|=KEY_DOWN;
        if(b&WPAD_CLASSIC_BUTTON_LEFT)r|=KEY_LEFT;if(b&WPAD_CLASSIC_BUTTON_RIGHT)r|=KEY_RIGHT;
        r|=axis(sinf(d->exp.classic.ljs.ang*0.0174532925f)*d->exp.classic.ljs.mag);
        if(b&(WPAD_CLASSIC_BUTTON_A|WPAD_CLASSIC_BUTTON_B|WPAD_CLASSIC_BUTTON_X|WPAD_CLASSIC_BUTTON_Y))r|=KEY_ACTION;
        if(b&WPAD_CLASSIC_BUTTON_PLUS)r|=KEY_PAUSE;if(b&WPAD_CLASSIC_BUTTON_HOME)r|=KEY_MENU;
    }else if(type==WPAD_EXP_NUNCHUK){
        if(b&WPAD_BUTTON_UP)r|=KEY_UP;if(b&WPAD_BUTTON_DOWN)r|=KEY_DOWN;float ny=cosf(d->exp.nunchuk.js.ang*0.0174532925f)*d->exp.nunchuk.js.mag;if(ny>0.4f)r|=KEY_UP;if(ny< -0.4f)r|=KEY_DOWN;
        r|=axis(sinf(d->exp.nunchuk.js.ang*0.0174532925f)*d->exp.nunchuk.js.mag);
        if(b&WPAD_BUTTON_LEFT)r|=KEY_LEFT;if(b&WPAD_BUTTON_RIGHT)r|=KEY_RIGHT;if(b&(WPAD_NUNCHUK_BUTTON_C|WPAD_NUNCHUK_BUTTON_Z))r|=KEY_ACTION;
    }else{if(b&WPAD_BUTTON_RIGHT)r|=KEY_UP;if(b&WPAD_BUTTON_LEFT)r|=KEY_DOWN;if(b&WPAD_BUTTON_UP)r|=KEY_LEFT;if(b&WPAD_BUTTON_DOWN)r|=KEY_RIGHT;}
    return r;
}
unsigned platform_input(void){
    WPAD_ScanPads();u32 pads=PAD_ScanPads();unsigned held[INPUT_DEVICES]={0},present=0;int ok=0;
    for(int i=0;i<8;i++){unsigned r=0;
        if(i<4)r=remote(i,&ok);else{int p=i-4;ok=!!(pads&(1u<<p));u32 b=PAD_ButtonsHeld(p);
            if(b&PAD_BUTTON_UP)r|=KEY_UP;if(b&PAD_BUTTON_DOWN)r|=KEY_DOWN;if(b&PAD_BUTTON_B)r|=KEY_BACK;if(PAD_StickY(p)>40)r|=KEY_UP;if(PAD_StickY(p)< -40)r|=KEY_DOWN;if(b&PAD_BUTTON_LEFT)r|=KEY_LEFT;if(b&PAD_BUTTON_RIGHT)r|=KEY_RIGHT;r|=axis(PAD_StickX(p)/100.0f);if(b&(PAD_BUTTON_A|PAD_BUTTON_B|PAD_BUTTON_X|PAD_BUTTON_Y))r|=KEY_ACTION;if(b&PAD_BUTTON_START)r|=KEY_PAUSE;if(b&PAD_TRIGGER_Z)r|=KEY_MENU;}
        if(ok){present|=1u<<i;held[i]=r;}
    }
    unsigned input=input_select(&controllers,held,present);if(quit||!SYS_MainLoop())input|=KEY_QUIT;return input;
}
int platform_connected(void){return controllers.connected;}
double platform_seconds(void){return ticks_to_microsecs(gettime())/1000000.0;}
void platform_aspect(void){if(wide)pillarbox=!pillarbox;}
int platform_wide(void){return wide;}
int platform_pillarbox(void){return wide&&pillarbox;}
int platform_240p(void){return low_resolution;}
void platform_scale(void){pixel_scale=pixel_scale?0:2;}
int platform_pixel_scale(void){return pixel_scale;}
void platform_present(Game*g){
    int tw=g->menu_frame?640:256,th=g->menu_frame?480:192;const uint16_t *src=g->menu_frame?g->menu_pixels:g->pixels;
    unsigned at=0;for(int y=0;y<th;y+=4)for(int x=0;x<tw;x+=4)for(int yy=0;yy<4;yy++)for(int xx=0;xx<4;xx++)texels[at++]=src[(y+yy)*tw+x+xx];
    DCFlushRange(texels,tw*th*2);GX_InvalidateTexAll();GXTexObj tex;GX_InitTexObj(&tex,texels,tw,th,GX_TF_RGB565,GX_CLAMP,GX_CLAMP,GX_FALSE);GX_InitTexObjLOD(&tex,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&tex,GX_TEXMAP0);
    Mtx44 p;guOrtho(p,0,480,0,640,-1,1);GX_LoadProjectionMtx(p,GX_ORTHOGRAPHIC);DisplayRect view=display_rect(wide,pillarbox,pixel_scale,g->menu_frame==CANVAS_FULLSCREEN);float w=view.w,h=view.h,x=view.x,y=view.y;
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);GX_Position3f32(x,y,0);GX_TexCoord2f32(0,0);GX_Position3f32(x+w,y,0);GX_TexCoord2f32(1,0);GX_Position3f32(x+w,y+h,0);GX_TexCoord2f32(1,1);GX_Position3f32(x,y+h,0);GX_TexCoord2f32(0,1);GX_End();front^=1;GX_CopyDisp(xfb[front],GX_TRUE);GX_DrawDone();VIDEO_SetNextFramebuffer(xfb[front]);
    // Never expose allocator contents or a framebuffer still being copied.
    if(!video_visible){VIDEO_SetBlack(FALSE);video_visible=1;}
    VIDEO_Flush();VIDEO_WaitVSync();
}
void platform_audio(Audio*a){
    if(!audio_started||ASND_StatusVoice(0)==SND_UNUSED){
        if(audio_started)audio_completed+=audio_segment_frames;
        int i=audio_pending>=0?audio_pending:0;
        if(audio_pending<0)audio_mix(a,audio_buffers[i],1024);
        DCFlushRange(audio_buffers[i],4096);
        ASND_SetVoice(0,VOICE_STEREO_16BIT,AUDIO_RATE,0,audio_buffers[i],4096,255,255,NULL);
        audio_pending=-1;audio_segment_frames=1024;audio_started=1;
        // ASND rejects AddVoice until the DSP has accepted SetVoice.
        return;
    }
    if(ASND_TestVoiceBufferReady(0)==1){
        if(audio_pending<0)for(int i=0;i<3;i++)if(ASND_TestPointer(0,audio_buffers[i])==0){
            audio_mix(a,audio_buffers[i],1024);DCFlushRange(audio_buffers[i],4096);audio_pending=i;break;
        }
        if(audio_pending>=0&&ASND_AddVoice(0,audio_buffers[audio_pending],4096)==SND_OK){audio_segment_frames+=1024;audio_pending=-1;}
    }
}
void platform_audio_reset(Audio *a){ASND_StopVoice(0);audio_started=0;audio_pending=-1;audio_completed=audio_segment_frames=0;a->mixed_frames=0;a->ui_end_frame=0;}
uint64_t platform_audio_played(void){
    uint64_t ticks=audio_started?ASND_GetTickCounterVoice(0):0;
    // The DSP counter includes its current 1024-sample output buffer.
    uint64_t frame=(ticks>1024?ticks-1024:0)*AUDIO_RATE/ASND_GetAudioRate();
    if(frame>audio_segment_frames)frame=audio_segment_frames;return audio_completed+frame;
}
void platform_capture(const char*p,Game*g){(void)p;(void)g;}
void platform_close(void){ASND_End();WPAD_Shutdown();VIDEO_SetBlack(TRUE);VIDEO_Flush();VIDEO_WaitVSync();}
