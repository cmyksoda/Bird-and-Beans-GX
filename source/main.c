// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static Game game;static Audio sound;
static const double start_fade_seconds=0.25;
static char savepath[1024];static int save_failed;
static void highs_read(void){unsigned char b[12];FILE *f=fopen(savepath,"rb");if(!f)return;if(fread(b,1,12,f)==12&&!memcmp(b,"BBS1",4))for(int i=0;i<2;i++){unsigned n=le32(b+4+i*4);if(n<=999990)game.high[i]=n;}fclose(f);}
static void highs_write(void){unsigned n=game_score(&game);if(n>game.high[game.mode])game.high[game.mode]=n;unsigned char b[12]={'B','B','S','1'};for(int i=0;i<2;i++)for(int j=0;j<4;j++)b[4+i*4+j]=game.high[i]>>(j*8);char tmp[1040];snprintf(tmp,sizeof(tmp),"%s.tmp",savepath);FILE*f=fopen(tmp,"wb");if(!f){save_failed=1;return;}int ok=fwrite(b,1,12,f)==12;if(fclose(f))ok=0;if(!ok){save_failed=1;return;}
    if(rename(tmp,savepath)==0){save_failed=0;return;}
    char bak[1040];snprintf(bak,sizeof(bak),"%s.bak",savepath);remove(bak);
    if(rename(savepath,bak)!=0){save_failed=1;return;}
    if(rename(tmp,savepath)==0){remove(bak);save_failed=0;}else{rename(bak,savepath);save_failed=1;}}
static void start(int mode,int deterministic){audio_stop(&sound);game_start(&game,mode,deterministic?0x12345678:(uint32_t)time(NULL));audio_commands(&sound,&game);}
// Keep controller history across scenes so a held button stays held.
static void title_music(void){audio_stop(&sound);audio_play(&sound,0,0);}
int main(int argc,char **argv){
    if(!platform_init())return 1;
    const char *pak="game.pak";
#ifdef HW_RVL
#ifdef EMBEDDED_GAME
    pak="sd:/apps/birdbeans-full/game.pak";
#else
    pak="sd:/apps/birdbeans/game.pak";
#endif
#endif
    int smoke=0;const char *capture=NULL;
    for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--smoke"))smoke=1;else if(!strcmp(argv[i],"--capture")&&i+1<argc)capture=argv[++i];else pak=argv[i];}
#ifdef HW_RVL
    char path[1024];snprintf(path,sizeof(path),"%s",argc?argv[0]:"");char *boot_slash=strrchr(path,'/');if(boot_slash){boot_slash[1]=0;strncat(path,"game.pak",sizeof(path)-strlen(path)-1);pak=path;}
#endif
    snprintf(savepath,sizeof(savepath),"%s",pak);char *slash=strrchr(savepath,'/');if(slash)strcpy(slash+1,"scores.dat");else strcpy(savepath,"scores.dat");
    int import_status=ROM_IMPORT_OK;
#ifdef EMBEDDED_GAME
    extern const unsigned char game_data[],game_data_end[];
    int loaded=game_load_memory(&game,game_data,game_data_end-game_data);
#else
    int loaded=game_load(&game,pak);
    if(!loaded){
        game_free(&game);
        ui_setup(&game,"PREPARING YOUR GAME","FIRST LAUNCH MAY TAKE A MOMENT.","KEEP THE SD CARD INSERTED.",1);
        platform_present(&game);
        import_status=rom_import(pak);
        if(import_status==ROM_IMPORT_OK)loaded=game_load(&game,pak);
    }
#endif
    if(!loaded||!audio_init(&sound,game.blobs[81])){
        fprintf(stderr,"BirdBeans import=%d: %s\n",import_status,game.vm.error);
        const char *heading="COULD NOT READ GAME DATA",*line1="REMOVE GAME.PAK TO REBUILD IT.",*line2="KEEP YOUR ROM BESIDE BOOT.DOL.";
        if(import_status==ROM_IMPORT_MISSING){heading="ROM NEEDED";line1="PUT YOUR USA .NDS ROM";line2="BESIDE BOOT.DOL, THEN RELAUNCH.";}
        else if(import_status==ROM_IMPORT_UNSUPPORTED){heading="UNSUPPORTED ROM";line1="USE THE UNMODIFIED USA RELEASE.";line2="OTHER REGIONS ARE NOT SUPPORTED.";}
        else if(import_status==ROM_IMPORT_IO){heading="COULD NOT PREPARE GAME";line1="CHECK THAT THE SD CARD IS WRITABLE.";line2="THEN RELAUNCH THE APP.";}
        else if(import_status==ROM_IMPORT_NOMEM){heading="NOT ENOUGH MEMORY";line1="RESTART THE APP AND TRY AGAIN.";line2="";}
        ui_setup(&game,heading,line1,line2,0);
        if(capture)platform_capture(capture,&game);
        do{platform_present(&game);}while(!(platform_input()&(KEY_MENU|KEY_QUIT))&&!smoke);game_free(&game);platform_close();audio_free(&sound);return 1;
    }
    char voicepath[1024];snprintf(voicepath,sizeof(voicepath),"%s",pak);char *voice_slash=strrchr(voicepath,'/');if(voice_slash)strcpy(voice_slash+1,"voices.pak");else strcpy(voicepath,"voices.pak");
    audio_load_voices(&sound,voicepath,(uint32_t)time(NULL)^(uint32_t)(platform_seconds()*1000000));
    platform_audio_reset(&sound);
    highs_read();int mode=0,menu=!smoke,pause=0,selection=0,graphics=0,graphics_selection=0,was_dead=0;unsigned prev=0,total=0,uiticks=0;
    start(mode,smoke);if(menu)title_music();
    double last=platform_seconds(),acc=0,uiacc=0,transition=0;int transition_phase=-1;unsigned transition_tick=0;const double step=1.0/59.8261;
    fprintf(stderr,"BirdBeans ready: mode %d\n",mode);
    while(1){
        unsigned held=platform_input();if(held&KEY_CONTROLLER_CHANGED){prev=0;game.previous=0;}
        unsigned down=held&~prev;prev=held;if(held&KEY_QUIT)break;
        double now=platform_seconds(),dt=now-last;last=now;if(dt>0.1)dt=0.1;if(dt<0)dt=0;acc+=dt;uiacc+=dt;
        while(uiacc>=step){uiticks++;uiacc-=step;}
        if(transition_phase>=0){
            if(transition_phase==1){
                if(sound.ui_end_frame!=UINT64_MAX&&platform_audio_played()>=sound.ui_end_frame){
                    audio_stop(&sound);platform_audio_reset(&sound);transition_phase=2;transition=0;
                }
            }else{
                transition+=dt;
                if(transition>=start_fade_seconds){
                    transition=0;
                    if(transition_phase==0){
                        transition_phase=1;platform_audio_reset(&sound);
                        if(!audio_start_voice(&sound))audio_play(&sound,AUDIO_UI_PLAYER,14);
                    }else{transition_phase=-1;menu=0;acc=0;platform_audio_reset(&sound);audio_commands(&sound,&game);}
                }
            }
        }else if(menu){
            if(down&KEY_MENU)break;
            // Up/down too, so an upright Wii Remote can switch games before its first A press.
            if(down&(KEY_LEFT|KEY_RIGHT|KEY_UP|KEY_DOWN)){mode^=1;audio_play(&sound,AUDIO_UI_PLAYER,13);}
            if((down&(KEY_ACTION|KEY_PAUSE))&&!(down&KEY_BACK)){
                pause=0;was_dead=0;audio_stop(&sound);platform_audio_reset(&sound);game_start(&game,mode,(uint32_t)time(NULL));
                // Let the device's already-mixed title audio drain before the silent fade.
                transition_phase=0;transition=-0.05;transition_tick=uiticks;acc=0;
                fprintf(stderr,"Started mode %d error=%s\n",mode,game.vm.error);
            }
        }else if(pause){
            if(graphics){
                int rows=platform_wide()?4:3;if(graphics_selection>=rows)graphics_selection=rows-1;
                if(down&KEY_UP){graphics_selection=(graphics_selection+rows-1)%rows;audio_play(&sound,AUDIO_UI_PLAYER,13);}
                if(down&KEY_DOWN){graphics_selection=(graphics_selection+1)%rows;audio_play(&sound,AUDIO_UI_PLAYER,13);}
                if(down&(KEY_PAUSE|KEY_MENU)){graphics=0;pause=0;audio_play(&sound,AUDIO_UI_PLAYER,12);acc=0;}
                else if(down&KEY_BACK){graphics=0;audio_play(&sound,AUDIO_UI_PLAYER,12);}
                else if(down&(KEY_ACTION|KEY_LEFT|KEY_RIGHT)){
                    if(graphics_selection==0)platform_toggle_240p();
                    else if(graphics_selection==1)platform_scale();
                    else if(platform_wide()&&graphics_selection==2)platform_aspect();
                    else if(down&KEY_ACTION)graphics=0;
                    audio_play(&sound,AUDIO_UI_PLAYER,13);
                }
            }else{
                if(down&(KEY_UP|KEY_LEFT)){selection=(selection+3)%4;audio_play(&sound,AUDIO_UI_PLAYER,13);}
                if(down&(KEY_DOWN|KEY_RIGHT)){selection=(selection+1)%4;audio_play(&sound,AUDIO_UI_PLAYER,13);}
                if(down&(KEY_PAUSE|KEY_BACK|KEY_MENU)){pause=0;audio_play(&sound,AUDIO_UI_PLAYER,12);acc=0;}
                else if(down&KEY_ACTION){
                    if(selection==0){pause=0;audio_play(&sound,AUDIO_UI_PLAYER,12);acc=0;}
                    else if(selection==1){highs_write();start(mode,0);audio_play(&sound,AUDIO_UI_PLAYER,12);pause=0;was_dead=0;acc=0;}
                    else if(selection==2){graphics=1;graphics_selection=0;audio_play(&sound,AUDIO_UI_PLAYER,13);}
                    else{highs_write();menu=1;pause=0;title_music();audio_play(&sound,AUDIO_UI_PLAYER,12);}
                }
            }
        }else if(game.dead||game.vm.fault){
            if(down&KEY_MENU){highs_write();menu=1;title_music();}
            else if(down&KEY_ACTION){highs_write();start(mode,0);was_dead=0;acc=0;}
        }else{
            if((down&(KEY_PAUSE|KEY_MENU))||!platform_connected()){pause=1;selection=0;graphics=0;audio_play(&sound,AUDIO_UI_PLAYER,11);}
        }
        if(smoke){held=((total/180)%2?32:16)|(total%90<65);acc=step;}
        if(!menu&&!pause&&transition_phase<0&&!game.vm.fault){while(acc>=step){if(!game_tick(&game,held&49))break;audio_commands(&sound,&game);acc-=step;total++;}}else acc=0;
        if(game.dead&&!was_dead&&!smoke){highs_write();was_dead=1;}
        sound.paused=pause||(!menu&&game.vm.fault);platform_audio(&sound);if(transition_phase<0)audio_commands(&sound,&game);
        if(transition_phase>=0)ui_transition(&game,mode,transition_tick,transition_phase,(float)(transition/start_fade_seconds));
        else if(menu)ui_title(&game,mode,uiticks);
        else{game_render(&game);if(game.vm.fault)ui_error(&game,"CORE ERROR - SEE CONSOLE");else if(pause){if(graphics)ui_graphics(&game,graphics_selection);else ui_pause(&game,selection);}else if(game.dead)ui_over(&game);}
        if(save_failed)text_draw(&game,20,179,"SCORE SAVE FAILED",0xf800);
        platform_present(&game);
        if(smoke&&total>=600){if(capture)platform_capture(capture,&game);printf("SMOKE frames=%u score=%u instructions=%u fault=%d\n",total,game_score(&game),game.vm.instructions,game.vm.fault);break;}
        if(smoke&&game.vm.fault){fprintf(stderr,"%s\n",game.vm.error);break;}
    }
    if(!smoke&&!menu)highs_write();int result=game.vm.fault?1:0;platform_close();audio_free(&sound);game_free(&game);return result;
}
