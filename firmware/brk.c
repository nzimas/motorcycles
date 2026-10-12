/* Live break: a breakcore-style variation of the playing pattern while PAGE is
 * held (see brk.h). Each press draws a new variation from its own seed:
 *
 * Rhythm, one recipe per press, applied to every track that has content:
 *   REPEAT  beat-repeat of 1/2/4/8 steps from the playhead at the press
 *   HALF    half-time drag from the press;  DOUBLE  double-time rush
 *   REVERSE each 8- or 16-step window played backwards
 *   CHOP    the bar cut into 2- or 4-step slices, reshuffled (amen chops)
 *   ROLL    the pattern as written, rolls building on the non-kick tracks
 * Movement, one or more layers on top:
 *   PAN     per-hit ping-pong/scatter; the kick stays centred on every quarter
 *   TIMBRE  COLOR/SWEEP (and SHAPE off the kick) drift around each sound
 *   FILTER  DJ filter sweeps (low- or high-pass) on the non-kick tracks
 *   DRIVE   distortion pushed harder on some tracks
 *   OCTAVE  octave jumps on non-kick hits (always in key); the kick never moves
 *   RATCHET extra ratchets sprinkled over the hits
 * No drops: tracks are never silenced, the kick keeps playing.
 * All functions are short and allocation-free: they run in the sequencer and
 * audio interrupts. The plan is written by the UI before `active` is set. */
#include "brk.h"

volatile mc_break mc_brk;

static uint32_t next(uint32_t *s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x?x:0x6d2b79f5u;}
static unsigned draw(uint32_t *s,unsigned n){return next(s)%n;}

void mc_break_begin(uint32_t seed,const uint8_t steps[6]) {
    mc_break p;
    uint32_t s=seed^0x9e3779b9u;
    unsigned char *z=(unsigned char *)&p;
    for(unsigned i=0;i<sizeof p;i++)z[i]=0;
    p.seed=seed;
    p.recipe=(uint8_t)draw(&s,BRK_RECIPES);
    static const uint8_t repeat_w[4]={1,2,4,8};
    p.window=p.recipe==BRK_REPEAT?repeat_w[draw(&s,4)]:p.recipe==BRK_REVERSE?(draw(&s,2)?8:16):p.recipe==BRK_CHOP?(draw(&s,2)?2:4):0;
    for(unsigned t=0;t<6;t++)p.anchor[t]=steps?steps[t]:0;
    if(p.recipe==BRK_CHOP) {
        unsigned n=16u/p.window;
        for(unsigned i=0;i<n;i++)p.chop[i]=(uint8_t)i;
        /* Keep the first slice (the downbeat) and shuffle about half the rest. */
        for(unsigned i=1;i<n;i++)if(draw(&s,100)<60){unsigned j=1+draw(&s,n-1);uint8_t x=p.chop[i];p.chop[i]=p.chop[j];p.chop[j]=x;}
    }
    /* Movement: at least one layer; ROLL always ratchets. */
    unsigned layers=0;
    for(unsigned k=0;k<6;k++)if(draw(&s,100)<35)layers|=1u<<k;
    if(!layers)layers=1u<<draw(&s,6);
    if(p.recipe==BRK_ROLL)layers|=BRK_RATCHET;
    p.layers=(uint8_t)layers;
    p.pan_width=(uint8_t)(24+draw(&s,36));
    p.timbre_depth=(uint8_t)(12+draw(&s,24));
    p.octave_odds=(uint8_t)(20+draw(&s,40));
    for(unsigned t=0;t<6;t++) {
        p.ratchet_odds[t]=(uint8_t)(layers&BRK_RATCHET?(t?20+draw(&s,50):draw(&s,12)):0);
        if(layers&BRK_FILTER&&t&&draw(&s,100)<75)p.filter_target[t]=(int8_t)(draw(&s,2)?-(int)(22+draw(&s,22)):(int)(18+draw(&s,22)));
        if(layers&BRK_DRIVE&&draw(&s,100)<60)p.drive_push[t]=(uint8_t)(25+draw(&s,50));
    }
    p.active=1;
    /* Publish: the interrupts only look at the plan once `active` is set. */
    volatile unsigned char *d=(volatile unsigned char *)&mc_brk;
    mc_brk.active=0;
    for(unsigned i=0;i<sizeof p;i++)if(i!=__builtin_offsetof(mc_break,active))d[i]=z[i];
    mc_brk.active=1;
}
void mc_break_end(void){mc_brk.active=0;}

unsigned mc_break_source(unsigned track,unsigned step,unsigned length) {
    if(!mc_brk.active||track>=6||!length||length>64||step>=length)return step;
    unsigned a=mc_brk.anchor[track]%length,w=mc_brk.window;
    /* Anchor on the beat: a repeat or tempo change starts from the last
       quarter (the last 8th-of-a-bar for 8-step repeats), where the kick is. */
    if(mc_brk.recipe==BRK_REPEAT||mc_brk.recipe==BRK_HALF||mc_brk.recipe==BRK_DOUBLE){unsigned q=w>4?8u:4u;a-=a%q;}
    unsigned rel=(step+length-a)%length;
    switch(mc_brk.recipe) {
    case BRK_REPEAT:return (a+rel%w)%length;
    case BRK_HALF:return (a+rel/2)%length;
    case BRK_DOUBLE:return (a+2*rel)%length;
    case BRK_REVERSE:{if(length<w)w=length;unsigned base=step-step%w;unsigned src=base+(w-1-step%w);return src<length?src:step;}
    case BRK_CHOP:{if(length<16)return step;unsigned bar=step-step%16,pos=step%16;unsigned src=bar+mc_brk.chop[pos/w]*w+pos%w;return src<length?src:step;}
    default:return step;
    }
}

unsigned mc_break_ratchet(unsigned track,unsigned *length_code) {
    if(!mc_brk.active||track>=6||!(mc_brk.layers&BRK_RATCHET))return 0;
    uint32_t s=mc_brk.seed^(track*0x85ebca6bu)^(mc_brk.hits[track]*0xc2b2ae35u);
    if(draw(&s,100)>=mc_brk.ratchet_odds[track])return 0;
    /* Rolls build up: the longer the hold, the faster (rate code 1..17 as the
       generator's retrig field; length code 1..2 steps). */
    unsigned build=mc_brk.blocks/750u;              /* ~0.5 s per stage */
    unsigned rate=(mc_brk.recipe==BRK_ROLL?6u:8u)+build*2u+draw(&s,3);
    if(rate>16)rate=16;
    *length_code=1+draw(&s,2);
    return rate+1;
}

int32_t mc_break_locks(unsigned track,uint8_t *b,int32_t n,int32_t cap,const uint8_t *sound,unsigned step) {
    if(!mc_brk.active||track>=6||!b)return n;
    uint32_t hit=mc_brk.hits[track]++;
    uint32_t s=mc_brk.seed^(track*0x27d4eb2fu)^(hit*0x165667b1u);
    unsigned layers=mc_brk.layers;
#define SOUND(k) (sound?(int)(*(const uint16_t *)(sound+20+2*(k))>>8):64)
    /* Set k to v (0..127): override an existing entry or append one. */
#define SET(k,v) do{int _v=(v);_v=_v<0?0:_v>127?127:_v;int32_t _i;for(_i=0;_i<n;_i++){uint16_t *_e=(uint16_t *)(b+20+8*_i);if(_e[0]==(k)){_e[1]=(uint16_t)(_v<<8);break;}} \
        if(_i==n&&n<cap){uint16_t *_e=(uint16_t *)(b+20+8*n);_e[0]=(k);_e[1]=(uint16_t)(_v<<8);_e[2]=0;_e[3]=0;n++;}}while(0)
    if(layers&BRK_PAN) {
        int w=mc_brk.pan_width;
        if(track==0)SET(22,step%4==0?64:(hit&1?64+w/3:64-w/3));
        else SET(22,draw(&s,3)?(hit&1?64+w:64-w):64-w+(int)draw(&s,2u*w+1));
    }
    if(layers&BRK_TIMBRE) {
        int d=mc_brk.timbre_depth;
        SET(11,SOUND(11)+(int)draw(&s,2u*d+1)-d);
        SET(13,SOUND(13)+(int)draw(&s,2u*d+1)-d);
        if(track)SET(12,SOUND(12)+(int)draw(&s,d+1)-d/2);
    }
    if(layers&BRK_OCTAVE&&track&&draw(&s,100)<mc_brk.octave_odds) {
        int p=SOUND(10)+(draw(&s,2)?12:-12);
        if(p<40)p+=24;
        if(p>88)p-=24;
        if(p>=40&&p<=88)SET(10,p);
    }
#undef SET
#undef SOUND
    return n;
}

void mc_break_audio(uint8_t params[6][8]) {
    if(!mc_brk.active)return;
    uint32_t b=mc_brk.blocks++;
    /* Sweeps reach their target in ~2 s (3000 blocks), from wherever the
       track's own filter sits. */
    unsigned f=b>3000u?3000u:b;
    for(unsigned t=0;t<6;t++) {
        if(mc_brk.filter_target[t]) {
            int from=(int8_t)params[t][6],to=mc_brk.filter_target[t];
            int v=from+(to-from)*(int)f/3000;
            params[t][6]=(uint8_t)(int8_t)(v==0?(to<0?-1:1):v);
            if(!params[t][7])params[t][7]=70;              /* a resonant sweep */
        }
        if(mc_brk.drive_push[t]) {
            unsigned a=params[t][0]+mc_brk.drive_push[t];
            params[t][0]=(uint8_t)(a>127?127:a);
        }
    }
}

#ifdef __m68k__
/* ---- Model:Cycles glue ------------------------------------------------------
 * Stock trig builder 0x400548ce(track, pattern, kit, step, reuse, out) reads
 * every per-step field from `step`; the sequencer interrupt (level 2) calls it
 * with reuse = 0 and out != 0 one step ahead of the playhead. Its entry is
 * hooked to remap the step (mc_brk_step) and its success exit to reshape the
 * built message (mc_brk_msg): msg +8 track, +40 flags (bit 15 retrig), +44
 * sound, +56 retrig rate index, +60 retrig length code, +64 velocity curve,
 * +72 lock buffer (count at +8, entries at +20 + 8 i). */
#include "duration.h"
static uint8_t length_codes[13];
static unsigned codes_ready;

unsigned mc_brk_step(unsigned track,const uint8_t *pattern,unsigned step) {
    if(!mc_brk.active||track>=6||!pattern)return step;
    unsigned length=pattern[30667]==1?((unsigned)pattern[722*track+713]<<8|pattern[722*track+714])
                                      :((unsigned)pattern[30662]<<8|pattern[30663]);
    if(!length||length>64||step>=length)return step;
    return mc_break_source(track,step,length);
}
void mc_brk_msg(uint8_t *msg) {
    if(!mc_brk.active||!msg)return;
    unsigned track=*(uint32_t *)(msg+8);
    if(track>=6)return;
    /* The step being built: the track's next step. */
    unsigned step=((const volatile uint8_t *)0x40a78860u)[track];
    uint8_t *locks=*(uint8_t **)(msg+72);
    if(locks) {
        int32_t n=*(int32_t *)(locks+8);
        if(n>=0&&n<=66)*(int32_t *)(locks+8)=mc_break_locks(track,locks,n,66,*(const uint8_t **)(msg+44),step);
    }
    unsigned length_code;unsigned rate=mc_break_ratchet(track,&length_code);
    if(rate&&codes_ready&&!(*(uint32_t *)(msg+40)&0x8000u)) {
        *(uint32_t *)(msg+40)|=0x8000u;
        *(int32_t *)(msg+56)=(int32_t)rate-1;
        *(int32_t *)(msg+60)=length_codes[length_code];
        *(int32_t *)(msg+64)=(int32_t)(mc_brk.seed>>(track*3)&31)-16;   /* gentle velocity curve */
    }
}
/* UI: PAGE held during playback. */
void mc_brk_press(uint32_t entropy) {
    if(!codes_ready)codes_ready=(unsigned)mc_duration_codes(2,(const uint32_t *)0x4010d814u,length_codes);
    uint8_t steps[6];
    for(unsigned t=0;t<6;t++)steps[t]=((const volatile uint8_t *)0x422ff168u)[t];
    static uint32_t presses;
    mc_break_begin(entropy*0x9e3779b9u+(++presses)*0x85ebca6bu,steps);
}
#endif
