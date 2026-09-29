// SPDX-License-Identifier: GPL-3.0-only
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

uint16_t le16(const void *p){const uint8_t *b=p;return b[0]|b[1]<<8;}
uint32_t le32(const void *p){const uint8_t *b=p;return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static uint32_t rd(Game *g,uint32_t a){return vm_read(&g->vm,a,4);}
static void wr(Game *g,uint32_t a,uint32_t b){vm_write(&g->vm,a,b,4);}
static void byte(Game *g,uint32_t a,uint8_t b){vm_write(&g->vm,a,b,1);}
static void release(Game *g,uint32_t p){
    if(p<SCENE+0x10020||p>=g->heap||rd(g,p-8)!=0x42424c4b)return;
    for(int i=0;i<256;i++)if(g->free_blocks[i]==p)return;
    for(int i=0;i<256;i++)if(!g->free_blocks[i]){g->free_blocks[i]=p;return;}
}
static uint32_t alloc(Game *g,unsigned n){
    n=(n+31)&~31u;uint32_t p=0;
    for(int i=0;i<256;i++)if(g->free_blocks[i]&&rd(g,g->free_blocks[i]-4)>=n){p=g->free_blocks[i];g->free_blocks[i]=0;break;}
    if(!p){p=g->heap+32;g->heap=p+n;
        if(g->heap>0x02300000){g->vm.fault=1;snprintf(g->vm.error,sizeof(g->vm.error),"object heap exhausted");return 0;}
        wr(g,p-8,0x42424c4b);wr(g,p-4,n);
    }
    memset(g->vm.mem+p-RAM_BASE,0,n);wr(g,p,SCENE+0x2000);return p;
}
static int resource(Game *g,uint32_t a){
    if(a<RAM_BASE||a>=RAM_BASE+RAM_SIZE-100)return -1;
    const char *s=(char *)g->vm.mem+a-RAM_BASE;
    const struct {const char *name;int id;} names[]={
        {"Pyoro/kyoro.ncl",3},{"Pyoro/kyoro_blue.ncl",5},{"Pyoro/kyoro_red.ncl",7},
        {"Pyoro/kyoro_red2.ncl",8},{"Pyoro/kyoro_dark.ncl",6},{"Pyoro/kyoro_yellow.ncl",10},
        {"Pyoro/kyoro_white.ncl",9},{"Pyoro/kyoro_black.ncl",4},
        {"Pyoro/kyoro.nsc",22},{"Pyoro/kyoro_sky.nsc",23},{"Pyoro/kyoro_title.nsc",28},
        {"Pyoro/kyoro_staff_en.nsc",24},{"Pyoro/kyoro_star.nsc",27},
        {"Pyoro/frame16x16.nsc",20},{"Pyoro/frame32x24.nsc",21},{"Pyoro/kyoro.ncg",12},
        {"Pyoro/kyoro_title.ncg",18}
    };
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);i++)if(!strcmp(s,names[i].name))return names[i].id;
    return -1;
}
static int layer(uint32_t mask){for(int i=0;i<4;i++)if(mask&(1u<<(8+i)))return i;return -1;}
static void sound(Game *g,int ch,int seq){
    if(g->sound_count<32){g->sound_channel[g->sound_count]=ch;g->sound_queue[g->sound_count++]=seq;}
}
#define PALETTE_RAM 0x02320000u
#define PALETTE_RES 0x02330000u
static uint32_t palette_address(unsigned mask){return PALETTE_RAM+(mask&0x1000?0:mask&0x2f00?512:mask&0x10?1024:1536);}
static uint32_t palette_resource(Game *g,uint32_t name){int id=resource(g,name);return id>=3&&id<=11?PALETTE_RES+id*512:0;}
static void palette_upload(Game *g,uint32_t src,unsigned mask,unsigned from,unsigned to,unsigned count){
    if(count==~0u)count=512;if(to>=512||from>=512)return;
    if(count>512-to)count=512-to;if(count>512-from)count=512-from;
    const unsigned masks[]={0x1000,0x2f00,0x10,0x2f};
    for(int i=0;i<4;i++)if(mask&masks[i])for(unsigned j=0;j<count;j++)byte(g,PALETTE_RAM+i*512+to+j,vm_read(&g->vm,src+from+j,1));
}
static int hook(VM *m,uint32_t a){
    Game *g=m->user;uint32_t x=m->r[0],y=m->r[1],z=m->r[2],t=m->r[3],ret=x;
    if((a>=0x02006c84&&a<0x0200a934)||(a>=0x020054b4&&a<0x0200599c)||
       (a>=0x020059b0&&a<0x020059c4)||(a>=0x020060f4&&a<0x02006138)||
       (a>=0x02005e64&&a<0x02005e9c)||
       (a>=0x02006154&&a<0x02006188)||(a>=0x02006198&&a<0x0200626c))return 0;
    switch(a){
    case 0x02080000:
        ret=y;
        if(rd(g,y)==0x0203630c||rd(g,y)==0x020359d0||rd(g,y)==0x02035ac8||rd(g,y)==0x02035a4c||rd(g,y)==0x02035c28){
            for(int i=0;i<MAX_TASKS;i++)if(!g->tasks[i].active){g->tasks[i]=(Task){y,1,0,0};break;}
        }else if(rd(g,y)==0x02035d94)g->gradient=y;break;
    case 0x02080004:break;
    case 0x02080008:case 0x0200599c:ret=0;break;
    case 0x0200d0a0:ret=alloc(g,x);break;
    case 0x0200cc34:wr(g,x,SCENE+0x2000);break;
    case 0x0200cfc0:
        for(int i=0;i<MAX_TASKS;i++)if(g->tasks[i].ptr==x&&g->tasks[i].active)g->tasks[i].done=1;
        break;
    case 0x0200d038:wr(g,x+36,y);wr(g,x+40,z);wr(g,x+44,t);wr(g,x+48,rd(g,m->r[13]));break;
    case 0x0200e320:
        wr(g,x+0xa8,z);vm_write(m,x+0xac,t,2);wr(g,x+0x44,rd(g,m->r[13])*4096);
        wr(g,x+0x48,rd(g,m->r[13]+4)*4096);byte(g,x+0x4c,1);wr(g,x+0x5c,rd(g,m->r[13]+8));byte(g,x+0xae,1);
        wr(g,x+0x64,4096);wr(g,x+0x68,0);
        wr(g,x+0x38,4096);wr(g,x+0x3c,4096);wr(g,x+0xbc,0x1010);
        wr(g,x+0xc0,rd(g,rd(g,0x0204cbd4)+0x219c));wr(g,x+0xc4,~0u);wr(g,x+0xcc,~0u);wr(g,x+0xd0,~0u);
        if(g->sprite_count<MAX_SPRITES)g->sprites[g->sprite_count++]=x;break;
    case 0x0200e48c:
        if(z||vm_read(m,x+0xac,2)!=y){
            vm_write(m,x+0xac,y,2);wr(g,x+0xd8,0);wr(g,x+0x58,0);
            // NNS_G2dInitCellAnimation restarts the controller when binding a sequence.
            wr(g,x+0x5c,1);wr(g,x+0x60,0);wr(g,x+0x64,4096);wr(g,x+0x68,0);
        }break;
    case 0x0200e4e8:wr(g,x+0xd8,y);wr(g,x+0x60,0);if(!z)wr(g,x+0x5c,0);break;
    case 0x0200e50c:wr(g,x+0x5c,y);if(y)byte(g,x+0x4c,1);break;
    case 0x0202f1fc:case 0x0202f22c:{uint64_t v=((uint64_t)y<<32)|x;v=a==0x0202f1fc?v<<(z&63):v>>(z&63);ret=v;m->r[1]=v>>32;break;}
    case 0x0200df18:sound(g,(x-SCENE-0x68)/64,0x10000|(y&127));break;
    case 0x0200df0c:sound(g,(x-SCENE-0x68)/64,0x20000|(y&65535));break;
    case 0x0200fd24:if((y|z)&0x3f00){g->blend_first=y>>8;g->blend_second=z>>8;g->blend_a=t;g->blend_b=rd(g,m->r[13]);}break;
    case 0x0200fdac:g->blend_a=y;g->blend_b=z;break;
    case 0x0200fc68:if(y&0x3f00)g->brightness=(int32_t)z;break;
    case 0x0200d424:wr(g,y,x&31);wr(g,z,(x>>5)&31);wr(g,t,(x>>10)&31);break;
    case 0x0200d1fc:{int n=rd(g,m->r[13]);ret=n?(int32_t)y+((int32_t)z-(int32_t)y)*(int32_t)t/n:z;break;}
    case 0x0200d2fc:{int pos=rd(g,m->r[13]);if(pos<(int)z)pos=z;if(pos>(int)t)pos=t;ret=t==z?y:(int32_t)x+((int32_t)y-(int32_t)x)*(pos-(int)z)/((int)t-(int)z);break;}
    case 0x02005a24:break;
    case 0x02025be0:for(unsigned i=0;i<z;i++)byte(g,x+i,y);break;
    case 0x02025b94:for(unsigned i=0;i<z;i++)byte(g,y+i,vm_read(m,x+i,1));break;
    case 0x0202f468:ret=y?x/y:0;m->r[1]=y?x%y:0;break;
    case 0x0202f25c:{int64_t sx=(int32_t)x,sy=(int32_t)y;ret=sy?(uint32_t)(sx/sy):0;m->r[1]=sy?(uint32_t)(sx%sy):0;break;}
    case 0x02005424:ret=g->high[g->mode];break;
    case 0x02005444:ret=0;break;
    case 0x020053e4:break;
    case 0x02005430:if(z>g->high[y&1])g->high[y&1]=z;break;
    case 0x0200dea0: sound(g,(x-SCENE-0x68)/64,y);wr(g,x+0x38,y);ret=1;break;
    case 0x0200def4:sound(g,(x-SCENE-0x68)/64,-1);break;
    case 0x0200dee0:ret=0;for(int i=0;i<16;i++)if(g->audio_active[i]&&g->audio_sequence[i]==(int)y)ret=1;break;
    case 0x0200df3c:{unsigned ch=(x-SCENE-0x68)/64;ret=ch<16?g->audio_ticks[ch]:0;break;}
    case 0x0200df24:sound(g,(x-SCENE-0x68)/64,0x40000|(y&255));break;
    case 0x0200f6bc:ret=palette_address(y);break;
    case 0x0200f630:ret=0x02310000+(layer(y)<0?0:layer(y))*4096;break;
    case 0x0201042c:for(int i=0;i<4;i++)if(y&(1u<<(8+i)))g->bg_scroll[i]=(int32_t)z;break;
    case 0x02010460:for(int i=0;i<4;i++)if(y&(1u<<(8+i)))g->bg_scroll[i]+=(int32_t)z;break;
    case 0x0200f0bc:palette_upload(g,x,y,z,t,rd(g,m->r[13]));break;
    case 0x0200f05c:ret=palette_resource(g,x);if(ret)palette_upload(g,ret,y,z,t,rd(g,m->r[13]));break;
    case 0x0200f0e4:ret=palette_resource(g,x);break;
    case 0x0200f110:vm_write(m,palette_address(y)+z,x,2);break;
    case 0x0201053c:for(int i=0;i<4;i++)if(y&(1u<<(8+i)))g->bg_priority[i]=z;break;
    case 0x0200df48:if(t<4)g->bank_screen[t]=z;{uint32_t mgr=rd(g,0x0204cbe0);wr(g,mgr+0x108,rd(g,mgr+0x108)|z);}break;
    case 0x0200f5c8:wr(g,x+0x108,rd(g,x+0x108)|y);break;
    case 0x0200f31c:{int id=resource(g,x),l=layer(y);if(l>=0&&id>=0&&g->blobs[id].size>=2084){
        memcpy(m->mem+0x310000+l*4096,g->blobs[id].p+36,2048);
        }break;}
    case 0x0200f37c:{int l=layer(y);(void)l;break;}
    case 0x0200e85c:{int id=resource(g,x),l=layer(y);if(l>=0&&id>=0)g->bg_graphics[l]=id;break;}
    // Native backends replace DS resource uploads, register writes and scene fades.
    case 0x02005458:case 0x0200680c:case 0x02010b44:
    case 0x0200e070:case 0x0200f128:case 0x0200e960:
    case 0x020054a0:break;
    case 0x0200d0d4:release(g,x);break;
    default:
        snprintf(m->error,sizeof(m->error),"unmapped service %08x from %08x",a,m->r[14]);m->fault=1;
    }
    m->r[0]=ret;return 1;
}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int b=0;b<8;b++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static int validate(Game *g){
    size_t n=g->pack_size;
    if(memcmp(g->pack,"BBP2",4)||le32(g->pack+4)!=82)goto invalid;
    for(unsigned i=0;i<82;i++){
        uint8_t *e=g->pack+8+i*12;uint32_t off=le32(e),len=le32(e+4);
        if(off<992||off>(unsigned)n||len>(unsigned)n-off||crc(g->pack+off,len)!=le32(e+8))goto invalid;
        g->blobs[i]=(Blob){g->pack+off,len};
    }
    if(g->blobs[80].size!=0x35000)goto invalid;
    g->vm.hook=hook;g->vm.user=g;return 1;
invalid:snprintf(g->vm.error,sizeof(g->vm.error),"Invalid or damaged game.pak");game_free(g);return 0;
}
int game_load_memory(Game *g,const void *data,size_t n){
    memset(g,0,sizeof(*g));if(n<992||n>8*1024*1024)return 0;
    g->pack=malloc(n);g->vm.mem=calloc(1,RAM_SIZE);g->pack_size=n;
    if(!g->pack||!g->vm.mem){game_free(g);return 0;}memcpy(g->pack,data,n);return validate(g);
}
int game_load(Game *g,const char *path){
    memset(g,0,sizeof(*g));FILE*f=fopen(path,"rb");if(!f){snprintf(g->vm.error,sizeof(g->vm.error),"game.pak missing");return 0;}
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}long n=ftell(f);rewind(f);
    if(n<992||n>8*1024*1024){fclose(f);return 0;}
    g->pack=malloc(n);g->vm.mem=calloc(1,RAM_SIZE);g->pack_size=n;
    if(!g->pack||!g->vm.mem||fread(g->pack,1,n,f)!=(size_t)n){fclose(f);game_free(g);return 0;}fclose(f);return validate(g);
}
int game_start(Game *g,int mode,uint32_t seed){
    VM *m=&g->vm;memset(m->mem,0,RAM_SIZE);memcpy(m->mem+0x4000,g->blobs[80].p,g->blobs[80].size);
    memset(m->r,0,sizeof(m->r));memset(g->tasks,0,sizeof(g->tasks));memset(g->free_blocks,0,sizeof(g->free_blocks));m->fault=0;m->error[0]=0;m->instructions=0;
    g->heap=SCENE+0x10000;g->mode=mode;g->frame=0;g->previous=0;g->sprite_count=0;g->dead=0;g->palette=mode?5:3;g->sound_count=0;g->gradient=0;g->blend_first=g->blend_second=0;g->blend_a=16;g->blend_b=0;g->brightness=0;
    for(int id=3;id<=11;id++){unsigned n=g->blobs[id].size>40?g->blobs[id].size-40:0;if(n>512)n=512;memcpy(m->mem+PALETTE_RES+id*512-RAM_BASE,g->blobs[id].p+40,n);}
    for(int i=0;i<4;i++){g->bg_graphics[i]=12;g->bg_priority[i]=0;g->bg_scroll[i]=0;memset(g->tilemap[i],0,sizeof(g->tilemap[i]));}
    for(int j=0;j<256;j+=4)wr(g,SCENE+0x2000+j,0x02080005);
    wr(g,SCENE+0x2000+0x60,0x02080001);wr(g,SCENE+0x2000+0x64,0x02080001);wr(g,SCENE+0x2000+0x6c,0x0200d039);wr(g,SCENE+0x2000+0x3c,0x02080009);
    wr(g,SCENE,SCENE+0x2000);wr(g,SCENE+0x48,STATE);
    for(uint32_t a=0x0204cbc0;a<0x0204cbe8;a+=4)wr(g,a,alloc(g,0x10000));
    // The original scene manager sets the default OBJ priority before constructing the game.
    wr(g,rd(g,0x0204cbd4)+0x219c,1);
    wr(g,0x0204cd84,seed);wr(g,0x0204cd88,0x19660d);wr(g,0x0204cd8c,0x3c6ef35f);
    byte(g,STATE+16,mode);
    // The scene start also starts the three synchronized music layers, two muted.
    if(!vm_call(m,0x02006c84,SCENE,0,0,0)||!vm_call(m,0x02007b88,SCENE,0,0,0))return 0;
    // Populate the original sky scanlines before the frozen start screen is rendered.
    if(g->gradient)vm_call(m,0x02006154,g->gradient,0,0,0);
    return !m->fault;
}
static void animations(Game *g){
    static const int banks[]={37,36,35,40};
    for(unsigned i=0;i<g->sprite_count;i++){
        uint32_t p=g->sprites[i];unsigned bank=rd(g,p+0xa8);if(bank>3||!rd(g,p+0x5c))continue;
        Blob b=g->blobs[banks[bank]];unsigned seq=vm_read(&g->vm,p+0xac,2);
        if(seq>=le16(b.p+24))continue;
        uint8_t *s=b.p+24+le32(b.p+28)+seq*16;unsigned count=le16(s);if(!count)continue;
        unsigned fr=rd(g,p+0xd8);if(fr>=count)fr=count-1;
        uint8_t *frames=b.p+24+le32(b.p+32)+le32(s+12);
        int speed=(int32_t)rd(g,p+0x64),reverse=!!rd(g,p+0x58);
        unsigned elapsed=rd(g,p+0x60)+(speed<0?-(int64_t)speed:speed);
        unsigned mode=rd(g,p+0x68);if(!mode)mode=le32(s+8);
        unsigned loop=le16(s+2);if(loop>=count)loop=0;
        while(rd(g,p+0x5c)){
            unsigned duration=le16(frames+fr*8+4)*4096u;
            if(!duration||elapsed<duration)break;
            elapsed-=duration;
            int forward=(speed>0)^reverse,next=(int)fr+(forward?1:-1);
            if(next>=(int)count||next<(int)loop){
                if(mode==3||mode==4){reverse^=1;wr(g,p+0x58,reverse);}
                if((mode==3||mode==4)&&next>=(int)loop)next=count-1;
                else if(mode==2||mode==4){next=((speed>0)^reverse)?loop:count-1;elapsed=0;}
                else{next=next<0?0:count-1;wr(g,p+0x5c,0);}
            }
            fr=next;
        }wr(g,p+0x60,elapsed);wr(g,p+0xd8,fr);
    }
}
int game_tick(Game *g,uint32_t input){
    VM *m=&g->vm;if(m->fault)return 0;g->sound_count=0;
    uint32_t k=rd(g,0x0204cbc0);wr(g,k+4,input&~g->previous);wr(g,k+8,input);g->previous=input;
    wr(g,SCENE+64,g->frame++);
    // The outer scene update applies music tempo and volume changes after gameplay.
    if(!g->dead)vm_call(m,0x02007b24,SCENE,0,0,0);
    else vm_call(m,0x0200a784,SCENE,0,0,0);
    for(int i=0;i<MAX_TASKS&&!m->fault;i++)if(g->tasks[i].active){
        uint32_t p=g->tasks[i].ptr;
        uint32_t vt=rd(g,p);
        if(!g->tasks[i].started){g->tasks[i].started=1;vm_call(m,rd(g,vt+8),p,0,0,0);}
        vm_call(m,rd(g,vt+16),p,0,0,0);
        if(g->tasks[i].done){vm_call(m,rd(g,vt+24),p,0,0,0);g->tasks[i].active=0;uint32_t cb=rd(g,p+40);
            if(cb)vm_call(m,cb,rd(g,p+36),rd(g,p+48),0,0);release(g,p);
        }
    }
    if(g->gradient)vm_call(m,0x02006154,g->gradient,0,0,0);
    animations(g);
    unsigned state=vm_read(m,STATE+72,1);
    if(state==5)g->dead=1;
    return !m->fault;
}
unsigned game_score(Game *g){return rd(g,STATE+0x2d0);}
void game_free(Game *g){free(g->vm.mem);g->vm.mem=NULL;free(g->pack);g->pack=NULL;}
