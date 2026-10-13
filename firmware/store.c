/* Store a live break as a pattern (RC21).
 *
 * While PAGE is held during playback (a live break, firmware/brk.c), the step
 * keys show the patterns of the current bank: lit = the pattern has trigs,
 * flashing = the pattern playing. Pressing a step key writes the variation as
 * heard into that pattern slot, overwriting it:
 *   - the steps the recipe plays (repeat, half/double time, reverse, chop),
 *   - the movement as step locks (pan, COLOR/SHAPE/SWEEP, octave pitch),
 *   - the ratchets as retrigs,
 *   - the filter sweeps (at their target) and distortion pushes in the
 *     tracks' sounds of the stored pattern.
 * The pattern and its kit are then copied and announced exactly as the stock
 * Paste Pattern does (0x40010a4a: two copies, owner and kit notifications),
 * so the OS saves them as usual and PATTERN + step loads them.
 *
 * Nothing here is in the payload: the code lives in dead CHORD caves
 * (adapter/cave.h) and the state and staging buffer in the RAM after the
 * payload's BSS (MC_STORE_STATE, MC_STORE_BUFFER, from the build). The
 * payload, and with it every audio routine's address, stays RC19's. */
#include <stdint.h>
#include "../adapter/cave.h"
#include "runtime.h"
#include "brk.h"
extern void mc_show_popup(const char *);
int mc_store_key(void *,const uint8_t *);
unsigned mc_store_light(unsigned,unsigned);
void mc_store_tick(void);

#define PATTERN_BYTES 30710u
#define KIT_BYTES     7094u            /* stock paste copies 0x1bb6 bytes of the kit */
#define TRACK_BYTES   722u
#define LOCKS         4332u
#define LOCK_TRACK    4385u
#define LOCK_STEP     68u
#define SLOT_USED     4352u
#define MAX_PAIRS     79u              /* stock saver 0x4005b95c keeps at most 79 (slot, track) pairs */
#define SOUND_AT(t)   (PATTERN_BYTES+28u+100u*(t))   /* kit +28: six 100-byte sounds */
#define DRIVE_AT      76u              /* sound +76: distortion record (firmware/drive_ui.c) */
#define MAGIC         0x53544f52u      /* 'STOR' */

typedef struct {
    uint32_t magic;
    uint16_t used;                     /* patterns of the bank with trigs */
    uint16_t swallow;                  /* step keys whose release/repeats are ours */
    uint8_t current;                   /* pattern playing at the press, 0..95 */
    uint8_t shown;                     /* LEDs drawn as: 0 stock, 1/2 flash phase */
} store_state;
#define ST  ((volatile store_state *)MC_STORE_STATE)
#define BUF ((uint8_t *)MC_STORE_BUFFER)

#define INLINE static inline __attribute__((always_inline))
INLINE uint32_t be16(const uint8_t *p){return (uint32_t)p[0]<<8|p[1];}
INLINE void put16(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}

/* ---- Stock objects (see architecture.md, binding notes) ------------------- */
INLINE uint8_t *app(void){return ((uint8_t *(*)(void))0x400cf866u)();}
INLINE uint8_t *pattern_owner(uint8_t *a,unsigned p){return a+5192+732*p;}
INLINE uint8_t *owner_data(uint8_t *owner){return (*(uint8_t *(**)(void *))(*(uint8_t **)owner+40))(owner);}
INLINE uint8_t *kit_data(uint8_t *a,unsigned p){uint8_t *k=a+116;return (*(uint8_t *(**)(void *,unsigned))(*(uint8_t **)k+48))(k,p);}

/* ---- Bake: the break plan applied to a copy of the pattern and kit -------- */
MC_CAVE_C static uint32_t next(uint32_t *s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x?x:0x6d2b79f5u;}
INLINE unsigned draw(uint32_t *s,unsigned n){return next(s)%n;}

/* As mc_break_source (firmware/brk.c). */
MC_CAVE_C static unsigned source(unsigned track,unsigned step,unsigned length) {
    unsigned a=mc_brk.anchor[track]%length,w=mc_brk.window,r=mc_brk.recipe;
    if(r==BRK_REPEAT||r==BRK_HALF||r==BRK_DOUBLE){unsigned q=w>4?8u:4u;a-=a%q;}
    unsigned rel=(step+length-a)%length,src=step;
    if(r==BRK_REPEAT)src=(a+rel%w)%length;
    else if(r==BRK_HALF)src=(a+rel/2)%length;
    else if(r==BRK_DOUBLE)src=(a+2*rel)%length;
    else if(r==BRK_REVERSE){if(length<w)w=length;src=step-step%w+(w-1-step%w);}
    else if(r==BRK_CHOP&&length>=16){unsigned pos=step%16;src=step-pos+mc_brk.chop[pos/w]*w+pos%w;}
    return src<length?src:step;
}
/* Lock slot k of one step to v (0..127), within the saver's pair budget. */
MC_CAVE_C static void set(uint8_t *step_locks,uint8_t *used,unsigned *pairs,unsigned k,int v) {
    v=v<0?0:v>127?127:v;
    if(!used[k]){if(*pairs>=MAX_PAIRS)return;used[k]=1;++*pairs;}
    if(be16(step_locks+2*k)==0xffffu)put16(step_locks+66,be16(step_locks+66)+1);
    put16(step_locks+2*k,(uint32_t)v<<8);
}
MC_CAVE_C static void bake(uint8_t *out,const uint8_t *in) {
    unsigned pairs=0;
    for(unsigned t=0;t<6;t++)for(unsigned k=0;k<33;k++)pairs+=out[LOCKS+LOCK_TRACK*t+SLOT_USED+k]!=0;
    for(unsigned t=0;t<6;t++) {
        const uint8_t *ti=in+TRACK_BYTES*t;uint8_t *to=out+TRACK_BYTES*t;
        unsigned length=in[30667]==1?be16(ti+713):be16(in+30662);
        if(!length||length>64)continue;
        /* Rhythm: each step takes the content of the step the recipe plays. */
        for(unsigned s=0;s<length;s++) {
            unsigned f=source(t,s,length);
            if(f==s)continue;
            to[2*s]=ti[2*f];to[2*s+1]=ti[2*f+1];
            for(unsigned at=128;at<=448;at+=64)to[at+s]=ti[at+f];   /* velocity, length, micro, retrig */
            for(unsigned at=516;at<=644;at+=64)to[at+s]=ti[at+f];   /* condition, note, sound lock */
            const uint8_t *li=in+LOCKS+LOCK_TRACK*t+LOCK_STEP*f;uint8_t *lo=out+LOCKS+LOCK_TRACK*t+LOCK_STEP*s;
            for(unsigned i=0;i<LOCK_STEP;i++)lo[i]=li[i];
        }
        /* Movement and ratchets, one draw per trig as the live break does. */
        uint8_t *used=out+LOCKS+LOCK_TRACK*t+SLOT_USED;
        const uint8_t *sound=out+SOUND_AT(t);
        unsigned layers=mc_brk.layers,hit=0;
        for(unsigned s=0;s<length;s++) {
            unsigned flags=be16(to+2*s);
            if(!(flags&1))continue;
            uint32_t h=hit++,r=mc_brk.seed^(t*0x27d4eb2fu)^(h*0x165667b1u);
            uint8_t *l=out+LOCKS+LOCK_TRACK*t+LOCK_STEP*s;
#define SOUND(k) ((int)sound[20+2*(k)])
            if(layers&BRK_PAN) {
                int w=mc_brk.pan_width;
                if(t==0)set(l,used,&pairs,22,s%4==0?64:(h&1?64+w/3:64-w/3));
                else set(l,used,&pairs,22,draw(&r,3)?(h&1?64+w:64-w):64-w+(int)draw(&r,2u*w+1));
            }
            if(layers&BRK_TIMBRE) {
                int d=mc_brk.timbre_depth;
                set(l,used,&pairs,11,SOUND(11)+(int)draw(&r,2u*d+1)-d);
                set(l,used,&pairs,13,SOUND(13)+(int)draw(&r,2u*d+1)-d);
                if(t)set(l,used,&pairs,12,SOUND(12)+(int)draw(&r,d+1)-d/2);
            }
            if(layers&BRK_OCTAVE&&t&&draw(&r,100)<mc_brk.octave_odds) {
                int p=SOUND(10)+(draw(&r,2)?12:-12);
                if(p<40)p+=24;
                if(p>88)p-=24;
                if(p>=40&&p<=88)set(l,used,&pairs,10,p);
            }
#undef SOUND
            /* Ratchets on note trigs only, never over a written retrig. Live,
               the draw follows the lock draw's hit count (h + 1). */
            uint32_t q=mc_brk.seed^(t*0x85ebca6bu)^((h+1)*0xc2b2ae35u);
            if(layers&BRK_RATCHET&&!(flags&0x8000u)&&!((flags&0x0800u)&&!(flags&0x0080u))&&draw(&q,100)<mc_brk.ratchet_odds[t]) {
                unsigned rate=(mc_brk.recipe==BRK_ROLL?6u:8u)+draw(&q,3);
                unsigned steps=1+draw(&q,2);
                put16(to+2*s,flags|0x8000u);
                to[384+s]=(uint8_t)rate;
                to[320+s]=steps==1?14:30;               /* stock length codes of 1 and 2 steps */
                to[448+s]=(uint8_t)((int)(mc_brk.seed>>(t*3)&31)-16);
            }
        }
        /* Filter sweeps at their target, distortion pushes added. */
        uint8_t *d=out+SOUND_AT(t)+DRIVE_AT;
        if(mc_brk.filter_target[t]){d[6]=(uint8_t)mc_brk.filter_target[t];if(!d[7])d[7]=70;}
        if(mc_brk.drive_push[t]){unsigned v=d[0]+mc_brk.drive_push[t];d[0]=(uint8_t)(v>127?127:v);}
    }
}

/* ---- Saving ---------------------------------------------------------------- */
#define COPY(d,s,n) ((void (*)(void *,const void *,unsigned))0x4008f1f0u)(d,s,n)
MC_CAVE_C_DATA static const char stored[]="STORED AS ";
MC_CAVE_C static int has_trigs(const uint8_t *pattern) {
    for(unsigned i=0;i<6*TRACK_BYTES;i+=TRACK_BYTES)
        for(unsigned s=0;s<64;s++)if(pattern[i+2*s+1]&1)return 1;
    return 0;
}
MC_CAVE_C static unsigned playing_pattern(void) {
    int i=((int (*)(void *))0x4006a96cu)(((void *(*)(void))0x400cfd0eu)());
    return i<0?0u:i>95?95u:(unsigned)i;
}
MC_CAVE_C static void scan(void) {
    uint8_t *a=app();
    if(!a)return;
    unsigned current=playing_pattern(),used=0;
    for(unsigned i=0;i<16;i++) {
        uint8_t *data=owner_data(pattern_owner(a,(current&~15u)+i));
        if(data&&has_trigs(data))used|=1u<<i;
    }
    ST->current=(uint8_t)current;ST->used=(uint16_t)used;
}
MC_CAVE_C static void store(unsigned slot) {
    uint8_t *a=app();
    if(!a)return;
    unsigned current=playing_pattern(),target=(current&~15u)+slot;
    uint8_t *owner=pattern_owner(a,target);
    uint8_t *from=owner_data(pattern_owner(a,current)),*to=owner_data(owner);
    uint8_t *from_kit=kit_data(a,current),*to_kit=kit_data(a,target);
    if(!from||!to||!from_kit||!to_kit)return;
    COPY(BUF,from,PATTERN_BYTES);COPY(BUF+PATTERN_BYTES,from_kit,KIT_BYTES);
    bake(BUF,from);
    /* The stock paste stamps the slot index in the copied pattern (+30706). */
    BUF[30706]=0;BUF[30707]=0;BUF[30708]=0;BUF[30709]=(uint8_t)target;
    COPY(to,BUF,PATTERN_BYTES);COPY(to_kit,BUF+PATTERN_BYTES,KIT_BYTES);
    (*(void (**)(void *,int))(*(uint8_t **)owner+16))(owner,0);
    uint8_t *k=a+116;
    (*(void (**)(void *,unsigned,int))(*(uint8_t **)k+92))(k,target,0);
    ST->used|=(uint16_t)(1u<<slot);
    char text[16];unsigned n=0;
    while(stored[n]){text[n]=stored[n];n++;}
    text[n++]=(char)('A'+target/16);text[n++]=(char)('0'+(slot+1)/10);text[n++]=(char)('0'+(slot+1)%10);text[n]=0;
    mc_show_popup(text);
}
/* Key dispatch, before mc_key: PAGE press while playing (outside generator
 * mode) surveys the bank; during a break, step keys store, and their repeats
 * and releases are consumed with them. */
MC_CAVE_B int mc_store_key(void *controller,const uint8_t *event) {
    (void)controller;
    unsigned code=(unsigned)event[15]|(unsigned)event[14]<<8,flags=event[19];
    volatile store_state *s=ST;
    if(s->magic!=MAGIC){s->magic=MAGIC;s->used=0;s->swallow=0;s->current=0;s->shown=0;}
    if(event[12]|event[13])return 0;
    if(code==15) {
        if((flags&1)&&!(flags&8)&&!mc_brk.active&&mc_native.ui.state==MC_INACTIVE&&
           ((unsigned (*)(void))0x4005481au)())scan();
        return 0;
    }
    if(code<16||code>=32)return 0;
    unsigned bit=1u<<(code-16);
    if(flags&16){if(s->swallow&bit){s->swallow&=(uint16_t)~bit;return 1;}return 0;}
    if(s->swallow&bit)return 1;
    if((flags&1)&&!(flags&8)&&mc_brk.active){s->swallow|=(uint16_t)bit;store(code-16);return 1;}
    return 0;
}

/* ---- LEDs (hot: every LED query and every LED pass) ------------------------ */
/* Flash phase from the free-running 135.168 MHz timer: about 4 Hz. */
#define PHASE() ((*(volatile uint32_t *)0xfc07000cu>>24&1u)+1u)
MC_CAVE_A0 unsigned mc_store_light(unsigned led,unsigned original) {
    if(!mc_brk.active||ST->magic!=MAGIC)return original;
    const uint32_t *map=*(const uint32_t **)0x404a8bdcu;
    if(map)for(unsigned i=0;i<16;i++)if(map[i]==led)
        return i==(ST->current&15u)?PHASE():ST->used>>i&1u?2u:1u;
    return original;
}
/* Repaint when the flash phase turns, and once when the break ends. */
MC_CAVE_B void mc_store_tick(void) {
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0":"=d"(sr));
    if(sr&0x0700)return;
    unsigned shown=mc_brk.active&&ST->magic==MAGIC?PHASE():0u;
    if(shown==ST->shown)return;
    ST->shown=(uint8_t)shown;
    void *view=((void *(*)(void *))0x400060d8u)(((void *(*)(void))0x400d0974u)());
    if(view)((void (*)(void *))0x40076c04u)(view);
}
