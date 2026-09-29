// SPDX-License-Identifier: GPL-3.0-only
// Decompression format reference: ndspy, Copyright 2019 RoadrunnerWMC.
// See licenses/ndspy.txt for upstream attribution and terms.
#include "rom.h"
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ROM_SIZE 1287168u
#define PACK_ENTRIES 82u
#define PACK_HEADER (8u + PACK_ENTRIES * 12u)
#define MAX_CHUNK 0x100000u

static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static int range(size_t size,size_t off,size_t n){return off<=size&&n<=size-off;}
static uint32_t rotr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}

static void sha_block(uint32_t *h,const uint8_t *b){
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64];
    for(unsigned i=0;i<16;i++)w[i]=(uint32_t)b[i*4]<<24|(uint32_t)b[i*4+1]<<16|(uint32_t)b[i*4+2]<<8|b[i*4+3];
    for(unsigned i=16;i<64;i++)w[i]=w[i-16]+(rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10));
    uint32_t a=h[0],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7],v=h[1];
    for(unsigned i=0;i<64;i++){
        uint32_t t=z+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        uint32_t u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&v)^(a&c)^(v&c));
        z=g;g=f;f=e;e=d+t;d=c;c=v;v=a;a=t+u;
    }
    h[0]+=a;h[1]+=v;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
}
static int supported(const uint8_t *data,size_t n){
    static const uint32_t expected[8]={0x9cd69777,0x61bc336e,0xe15d1178,0x339b6dbf,0xbe66448a,0x26767c40,0x09209fa5,0xb6a17d6a};
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t whole=n&~(size_t)63;for(size_t i=0;i<whole;i+=64)sha_block(h,data+i);
    uint8_t tail[128]={0};size_t rest=n-whole,bytes=rest<56?64:128;memcpy(tail,data+whole,rest);tail[rest]=0x80;
    uint64_t bits=(uint64_t)n*8;for(unsigned i=0;i<8;i++)tail[bytes-1-i]=bits>>(i*8);
    sha_block(h,tail);if(bytes==128)sha_block(h,tail+64);
    return !memcmp(h,expected,sizeof h);
}
static uint32_t crc32(const uint8_t *p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}

static int lz10(const uint8_t *in,size_t size,uint8_t **result,size_t *length){
    if(size<4||in[0]!=0x10)return ROM_IMPORT_INVALID;
    size_t n=le32(in)>>8;if(!n||n>MAX_CHUNK)return ROM_IMPORT_INVALID;
    uint8_t *out=malloc(n);if(!out)return ROM_IMPORT_NOMEM;
    size_t src=4,dst=0;
    while(dst<n){
        if(src>=size)goto invalid;unsigned flags=in[src++];
        for(unsigned mask=128;mask&&dst<n;mask>>=1){
            if(flags&mask){
                if(!range(size,src,2))goto invalid;
                unsigned token=(unsigned)in[src]<<8|in[src+1];src+=2;
                size_t distance=(token&4095)+1,count=(token>>12)+3;
                if(distance>dst)goto invalid;
                while(count--&&dst<n){out[dst]=out[dst-distance];dst++;}
            }else{if(src>=size)goto invalid;out[dst++]=in[src++];}
        }
    }
    *result=out;*length=n;return ROM_IMPORT_OK;
invalid:free(out);return ROM_IMPORT_INVALID;
}

static int arm9_decode(const uint8_t *in,size_t size,uint8_t **result,size_t *length){
    if(size<8)return ROM_IMPORT_INVALID;
    uint32_t footer=le32(in+size-8);size_t header=footer>>24,compressed=footer&0xffffff,extra=le32(in+size-4);
    if(header<8||header>compressed||compressed>size||extra>MAX_CHUNK||size>MAX_CHUNK-extra)return ROM_IMPORT_INVALID;
    for(size_t i=size-header;i<size-8;i++)if(in[i]!=255)return ROM_IMPORT_INVALID;
    size_t n=size+extra,plain=size-compressed,src=size-header,dst=n;
    uint8_t *out=calloc(1,n);if(!out)return ROM_IMPORT_NOMEM;memcpy(out,in,plain);
    while(dst>plain){
        if(src<=plain)goto invalid;unsigned flags=in[--src];
        for(unsigned mask=128;mask&&dst>plain;mask>>=1){
            if(flags&mask){
                if(src-plain<2)goto invalid;unsigned token=(unsigned)in[src-1]<<8|in[src-2];src-=2;
                size_t count=(token>>12)+3,distance=(token&4095)+3;
                if(distance>n-dst)goto invalid;
                if(count>dst-plain)goto invalid;
                while(count--){dst--;out[dst]=out[dst+distance];}
            }else{if(src<=plain)goto invalid;out[--dst]=in[--src];}
        }
    }
    *result=out;*length=n;return ROM_IMPORT_OK;
invalid:free(out);return ROM_IMPORT_INVALID;
}

static int member(const uint8_t *rom,unsigned id,const uint8_t **data,size_t *n){
    size_t fat=le32(rom+0x48),fat_size=le32(rom+0x4c);
    if(!range(ROM_SIZE,fat,fat_size)||!range(fat_size,id*8,8))return 0;
    size_t start=le32(rom+fat+id*8),end=le32(rom+fat+id*8+4);
    if(end<start||!range(ROM_SIZE,start,end-start))return 0;
    *data=rom+start;*n=end-start;return 1;
}
static int append(FILE *file,uint8_t *table,unsigned id,uint32_t *offset,const uint8_t *data,size_t n){
    put32(table+8+id*12,*offset);put32(table+12+id*12,n);put32(table+16+id*12,crc32(data,n));
    if(fwrite(data,1,n,file)!=n)return 0;*offset+=n;return 1;
}
static int prepare(const uint8_t *rom,const char *pack){
    const uint8_t *narc,*audio;size_t narc_size,audio_size;
    /* File IDs are fixed only after the complete supported ROM hash matches. */
    if(!member(rom,0,&narc,&narc_size)||!member(rom,3,&audio,&audio_size))return ROM_IMPORT_INVALID;
    if(narc_size<28||memcmp(narc,"NARC",4)||memcmp(narc+16,"BTAF",4)||le32(narc+24)!=80)return ROM_IMPORT_INVALID;
    size_t fat_size=le32(narc+20),fnt=16+fat_size;
    if(fat_size<12+80*8||!range(narc_size,fnt,8)||memcmp(narc+fnt,"BTNF",4))return ROM_IMPORT_INVALID;
    size_t fnt_size=le32(narc+fnt+4);if(fnt_size<8||!range(narc_size,fnt,fnt_size))return ROM_IMPORT_INVALID;
    size_t image=fnt+fnt_size;if(!range(narc_size,image,8)||memcmp(narc+image,"GMIF",4))return ROM_IMPORT_INVALID;image+=8;
    char temp[1040];if(snprintf(temp,sizeof temp,"%s.tmp",pack)>=(int)sizeof temp)return ROM_IMPORT_IO;
    FILE *file=fopen(temp,"wb");if(!file)return ROM_IMPORT_IO;
    uint8_t table[PACK_HEADER]={0};memcpy(table,"BBP2",4);put32(table+4,PACK_ENTRIES);
    uint32_t offset=PACK_HEADER;int status=ROM_IMPORT_IO;uint8_t *chunk=NULL;size_t length=0;
    if(fwrite(table,1,sizeof table,file)!=sizeof table)goto finish;
    for(unsigned i=0;i<80;i++){
        size_t first=le32(narc+28+i*8),last=le32(narc+32+i*8);
        status=ROM_IMPORT_INVALID;if(last<first||!range(narc_size-image,first,last-first))goto finish;
        status=lz10(narc+image+first,last-first,&chunk,&length);if(status!=ROM_IMPORT_OK)goto finish;
        int ok=append(file,table,i,&offset,chunk,length);free(chunk);chunk=NULL;
        if(!ok){status=ROM_IMPORT_IO;goto finish;}
    }
    size_t arm_start=le32(rom+0x20),arm_size=le32(rom+0x2c);
    status=ROM_IMPORT_INVALID;if(!range(ROM_SIZE,arm_start,arm_size))goto finish;
    status=arm9_decode(rom+arm_start,arm_size,&chunk,&length);if(status!=ROM_IMPORT_OK)goto finish;
    if(length<0x35000){status=ROM_IMPORT_INVALID;goto finish;}
    status=ROM_IMPORT_IO;
    if(!append(file,table,80,&offset,chunk,0x35000)||!append(file,table,81,&offset,audio,audio_size))goto finish;
    if(fseek(file,0,SEEK_SET)||fwrite(table,1,sizeof table,file)!=sizeof table)goto finish;
    status=ROM_IMPORT_OK;
finish:
    free(chunk);if(fclose(file))status=ROM_IMPORT_IO;
    if(status==ROM_IMPORT_OK&&rename(temp,pack)){
        // libfat cannot rename over an existing cache; retain it until replacement succeeds.
        char backup[1040];snprintf(backup,sizeof backup,"%s.bak",pack);remove(backup);
        if(rename(pack,backup))status=ROM_IMPORT_IO;
        else if(rename(temp,pack)){rename(backup,pack);status=ROM_IMPORT_IO;}
        else remove(backup);
    }
    if(status!=ROM_IMPORT_OK)remove(temp);
    return status;
}
int rom_extract(const char *rom_path,const char *pack_path){
    size_t pack_length=strlen(pack_path);
    if(!strcmp(rom_path,pack_path)||(pack_length>=4&&!strcasecmp(pack_path+pack_length-4,".nds")))return ROM_IMPORT_IO;
    FILE *file=fopen(rom_path,"rb");if(!file)return ROM_IMPORT_IO;
    if(fseek(file,0,SEEK_END)){fclose(file);return ROM_IMPORT_IO;}
    long size=ftell(file);rewind(file);if(size!=ROM_SIZE){fclose(file);return ROM_IMPORT_UNSUPPORTED;}
    uint8_t *data=malloc(ROM_SIZE);if(!data){fclose(file);return ROM_IMPORT_NOMEM;}
    int status=fread(data,1,ROM_SIZE,file)==ROM_SIZE?ROM_IMPORT_OK:ROM_IMPORT_IO;
    if(fclose(file))status=ROM_IMPORT_IO;
    if(status==ROM_IMPORT_OK)status=supported(data,ROM_SIZE)?prepare(data,pack_path):ROM_IMPORT_UNSUPPORTED;
    free(data);return status;
}
int rom_import(const char *pack_path){
    char folder[1024];if(strlen(pack_path)>=sizeof folder)return ROM_IMPORT_IO;
    strcpy(folder,pack_path);char *slash=strrchr(folder,'/');
    if(slash)slash[1]=0;else strcpy(folder,"./");
    DIR *directory=opendir(folder);if(!directory)return ROM_IMPORT_IO;
    struct dirent *entry;int result=ROM_IMPORT_MISSING;
    while((entry=readdir(directory))){
        size_t n=strlen(entry->d_name);if(n<4||strcasecmp(entry->d_name+n-4,".nds"))continue;
        char path[1040];if(snprintf(path,sizeof path,"%s%s",folder,entry->d_name)>=(int)sizeof path){result=ROM_IMPORT_IO;continue;}
        result=rom_extract(path,pack_path);if(result!=ROM_IMPORT_UNSUPPORTED)break;
    }
    closedir(directory);return result;
}
