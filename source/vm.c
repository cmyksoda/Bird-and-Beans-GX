// SPDX-License-Identifier: GPL-3.0-only
#include "vm.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static void fail(VM *m, const char *why, uint32_t value) {
    if (!m->fault) snprintf(m->error,sizeof(m->error),"%s %08x at %08x",why,value,m->pc);
    m->fault=1;
}
uint32_t vm_read(VM *m,uint32_t a,unsigned n) {
    if(a<RAM_BASE || a-RAM_BASE>RAM_SIZE-n) {fail(m,"read",a);return 0;}
    uint32_t v=0; for(unsigned i=0;i<n;i++)v|=(uint32_t)m->mem[a-RAM_BASE+i]<<(8*i);
    return v;
}
void vm_write(VM *m,uint32_t a,uint32_t v,unsigned n) {
    if(a<RAM_BASE || a-RAM_BASE>RAM_SIZE-n) {fail(m,"write",a);return;}
    for(unsigned i=0;i<n;i++)m->mem[a-RAM_BASE+i]=(uint8_t)(v>>(8*i));
}
static uint32_t nz(VM *m,uint32_t v) {m->n=v>>31;m->z=v==0;return v;}
static uint32_t add(VM *m,uint32_t a,uint32_t b,unsigned c) {
    uint64_t sum=(uint64_t)a+b+c;
    int64_t signed_sum=(int64_t)(int32_t)a+(int32_t)b+c;
    m->c=sum>>32;m->v=signed_sum>INT32_MAX||signed_sum<INT32_MIN;
    return nz(m,(uint32_t)sum);
}
static uint32_t shift(VM *m,uint32_t a,unsigned n,unsigned op) {
    if(!n)return nz(m,a);
    if(op==0) {m->c=n<=32?(a>>(32-n))&1:0;a=n<32?a<<n:0;}
    if(op==1) {m->c=n<=32?(a>>(n-1))&1:0;a=n<32?a>>n:0;}
    if(op==2) {m->c=n<32?(a>>(n-1))&1:a>>31;a=(uint32_t)((int32_t)a>>(n<32?n:31));}
    if(op==3) {n&=31;if(n)a=(a>>n)|(a<<(32-n));m->c=a>>31;}
    return nz(m,a);
}
static int cond(VM *m,unsigned x) {
    switch(x) {
    case 0:return m->z;case 1:return !m->z;case 2:return m->c;case 3:return !m->c;
    case 4:return m->n;case 5:return !m->n;case 6:return m->v;case 7:return !m->v;
    case 8:return m->c&&!m->z;case 9:return !m->c||m->z;case 10:return m->n==m->v;
    case 11:return m->n!=m->v;case 12:return !m->z&&m->n==m->v;case 13:return m->z||m->n!=m->v;
    default:return 0;
    }
}
static uint32_t reg(VM *m,unsigned r) {return r==15?m->pc+4:m->r[r];}
static void step(VM *m) {
    uint32_t pc=m->pc,next=pc+2,*r=m->r;
    unsigned h=vm_read(m,pc,2),d=h&7,s=(h>>3)&7,t=(h>>6)&7;
    if((h&0xf800)<0x1800) {
        unsigned op=h>>11,n=(h>>6)&31;if(!n&&op)n=32;
        r[d]=shift(m,r[s],n,op);
    } else if((h&0xf800)==0x1800) {
        uint32_t b=(h&0x400)?t:r[t];r[d]=(h&0x200)?add(m,r[s],~b,1):add(m,r[s],b,0);
    } else if((h&0xe000)==0x2000) {
        unsigned op=(h>>11)&3;d=(h>>8)&7;uint32_t b=h&255;
        if(op==0)r[d]=nz(m,b);if(op==1)add(m,r[d],~b,1);
        if(op==2)r[d]=add(m,r[d],b,0);if(op==3)r[d]=add(m,r[d],~b,1);
    } else if((h&0xfc00)==0x4000) {
        uint32_t a=r[d],b=r[s];
        switch((h>>6)&15) {
        case 0:r[d]=nz(m,a&b);break;case 1:r[d]=nz(m,a^b);break;
        case 2:case 3:case 4:r[d]=shift(m,a,b&255,((h>>6)&15)-2);break;
        case 5:r[d]=add(m,a,b,m->c);break;case 6:r[d]=add(m,a,~b,m->c);break;
        case 7:r[d]=shift(m,a,b&255,3);break;case 8:nz(m,a&b);break;
        case 9:r[d]=add(m,0,~b,1);break;case 10:add(m,a,~b,1);break;
        case 11:add(m,a,b,0);break;case 12:r[d]=nz(m,a|b);break;
        case 13:r[d]=nz(m,a*b);break;case 14:r[d]=nz(m,a&~b);break;case 15:r[d]=nz(m,~b);break;
        }
    } else if((h&0xfc00)==0x4400) {
        d=(h&7)|((h>>4)&8);s=(h>>3)&15;uint32_t b=reg(m,s);unsigned op=(h>>8)&3;
        if(op==0){b+=reg(m,d);if(d==15)next=b&~1u;else r[d]=b;}
        if(op==1)add(m,reg(m,d),~b,1);
        if(op==2){if(d==15)next=b&~1u;else r[d]=b;}
        if(op==3){if(h&128)r[14]=(pc+2)|1;next=b&~1u;}
    } else if((h&0xf800)==0x4800) {
        r[(h>>8)&7]=vm_read(m,((pc+4)&~3u)+(h&255)*4,4);
    } else if((h&0xf000)==0x5000) {
        uint32_t a=r[s]+r[(h>>6)&7];
        switch((h>>9)&7){
        case 0:vm_write(m,a,r[d],4);break;case 1:vm_write(m,a,r[d],2);break;
        case 2:vm_write(m,a,r[d],1);break;case 3:r[d]=(int8_t)vm_read(m,a,1);break;
        case 4:r[d]=vm_read(m,a,4);break;case 5:r[d]=vm_read(m,a,2);break;
        case 6:r[d]=vm_read(m,a,1);break;case 7:r[d]=(int16_t)vm_read(m,a,2);break;
        }
    } else if((h&0xe000)==0x6000) {
        unsigned n=h&0x1000?1:4;uint32_t a=r[s]+((h>>6)&31)*n;
        if(h&0x800)r[d]=vm_read(m,a,n);else vm_write(m,a,r[d],n);
    } else if((h&0xf000)==0x8000) {
        uint32_t a=r[s]+((h>>6)&31)*2;
        if(h&0x800)r[d]=vm_read(m,a,2);else vm_write(m,a,r[d],2);
    } else if((h&0xf000)==0x9000) {
        uint32_t a=r[13]+(h&255)*4;d=(h>>8)&7;
        if(h&0x800)r[d]=vm_read(m,a,4);else vm_write(m,a,r[d],4);
    } else if((h&0xf000)==0xa000) {
        r[(h>>8)&7]=((h&0x800)?r[13]:((pc+4)&~3u))+(h&255)*4;
    } else if((h&0xff00)==0xb000) {
        unsigned n=(h&127)*4;r[13]=(h&128)?r[13]-n:r[13]+n;
    } else if((h&0xf600)==0xb400) {
        if(h&0x800){for(unsigned i=0;i<8;i++)if(h&(1u<<i)){r[i]=vm_read(m,r[13],4);r[13]+=4;}
            if(h&256){next=vm_read(m,r[13],4)&~1u;r[13]+=4;}
        } else {if(h&256){r[13]-=4;vm_write(m,r[13],r[14],4);}
            for(int i=7;i>=0;i--)if(h&(1u<<i)){r[13]-=4;vm_write(m,r[13],r[i],4);}}
    } else if((h&0xf000)==0xc000) {
        s=(h>>8)&7;uint32_t a=r[s];for(unsigned i=0;i<8;i++)if(h&(1u<<i)){
            if(h&0x800)r[i]=vm_read(m,a,4);else vm_write(m,a,r[i],4);a+=4;
        }if(!(h&0x800)||!(h&(1u<<s)))r[s]=a;
    } else if((h&0xf000)==0xd000 && ((h>>8)&15)<14) {
        if(cond(m,(h>>8)&15))next=pc+4+(int8_t)(h&255)*2;
    } else if((h&0xf800)==0xe000) {
        next=pc+4+(int32_t)((h&2047)^1024)*2-2048;
    } else if((h&0xf800)==0xf000) {
        unsigned h2=vm_read(m,pc+2,2);int32_t off=(int32_t)((h&2047)^1024)-1024;
        next=pc+4+off*4096+(h2&2047)*2;r[14]=(pc+4)|1;
        if((h2&0xf800)==0xe800)next&=~3u;
        else if((h2&0xf800)!=0xf800)fail(m,"bad BL",h2);
    } else fail(m,"unsupported opcode",h);
    m->pc=next;
}
int vm_call(VM *m,uint32_t addr,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
    if(m->fault)return 0;
    m->r[0]=a;m->r[1]=b;m->r[2]=c;m->r[3]=d;
    m->r[13]=RAM_BASE+RAM_SIZE-4096;m->r[14]=RAM_BASE+0x90001;m->pc=addr&~1u;
    unsigned steps=0;
    while(m->pc!=RAM_BASE+0x90000&&!m->fault) {
        if(++steps>1000000){fail(m,"instruction budget",addr);break;}
        if(m->hook&&m->hook(m,m->pc)){m->pc=m->r[14]&~1u;continue;}
        step(m);
    }
    m->instructions+=steps;
    return !m->fault;
}
