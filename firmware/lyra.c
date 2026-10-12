/* LYRA: replaces the stock CHORD machine (machine 5).
 *
 * An economical voice after SOMA's LYRA-8 (and Mike Moreno's LIRA-8 / xmacex's
 * LNRA-8): four oscillators in two pairs, each morphing from triangle to
 * square ("sharpness"), cross-modulating within the pair and from one pair to
 * the other through their previous outputs (feedback FM, from clean to
 * chaotic), the pairs detuned against each other or set at an interval, and
 * a "hyper LFO" that breathes the modulation and the second pair's pitch, from
 * slow drift to audio-rate growl.
 *
 * Knobs (8.8 words of the track parameters):
 *   PITCH, FINE, note : the pitch, as every stock machine (440 Hz at note 69)
 *   COLOR   "Spread"  : 0..63 detune between the four oscillators (0 to ~2.5 %,
 *                       slow beating to wide chorus); 64..127 the second pair
 *                       at a fifth, an octave, an octave and a fifth, two
 *                       octaves (16 steps each), slightly detuned
 *   SHAPE   "Sharp"   : triangle (0) to near square (127)
 *   SWEEP   "Mod"     : cross-modulation depth, 0 = clean
 *   CONTOUR "Hyper"   : hyper-LFO rate, 0 = off, 0.05 Hz .. 40 Hz
 *   DECAY, GATE, PUNCH: the stock amp chain, set up as TONE does (as MACRO)
 *
 * Cost: about 80 ColdFire instructions per sample while sounding, nothing
 * beyond a level check while silent (CHORD runs all its voices every block).
 * Integer only; every intermediate fits 32 bits, phase arithmetic wraps
 * unsigned.
 */
#include <stdint.h>
#include "lyra.h"
#include "lyra_tables.h"

static int32_t clamp15(int32_t v){return v>32767?32767:v<-32767?-32767:v;}
static int32_t clamp17(int32_t v){return v>131071?131071:v<-131071?-131071:v;}
/* inc * (1 + q / 65536) for q in roughly -2^15..2^16, within 32 bits. */
static uint32_t scale(uint32_t inc,int32_t q){return (inc>>8)*(uint32_t)(256+(q>>8))+((inc&255)*(uint32_t)(256+(q>>8))>>8);}
static uint32_t ratio(uint32_t inc,uint32_t r8){return (inc>>8)*r8+((inc&255)*r8>>8);}   /* r8: ratio x 256 */

void lyra_block(lyra_state *s,const lyra_params *p,int32_t out[32]) {
    /* Base increment, linear between semitones. */
    int32_t note=p->note<0?0:p->note>(127<<16)?127<<16:p->note;
    unsigned n=(unsigned)note>>16,frac=(unsigned)note&0xffff;
    uint32_t a=lyra_note_inc[n],b=lyra_note_inc[n+1];
    uint32_t inc=a+((b-a)>>8)*(frac>>8);
    /* Spread: detune d (Q16) and the second pair's ratio (x256). */
    unsigned c=p->spread>127?127:p->spread;int32_t d;uint32_t r8;
    if(c<64){d=(int32_t)(c*c*1638u/3969u);r8=256;}
    else{static const uint16_t ratios[4]={384,512,768,1024};d=393;r8=ratios[(c-64)/16];}
    /* Hyper LFO: block-rate triangle, -32767..32767. */
    unsigned h=p->hyper>127?127:p->hyper;int32_t l=0;
    if(h){s->lfo+=lyra_lfo_inc[h];int32_t t=(int32_t)s->lfo;l=((t^(t>>31))>>15)-32768;}
    else s->lfo=0;
    uint32_t i0=inc,i1=scale(inc,d),b0=ratio(inc,r8);
    int32_t vib=h?(l*262)>>15:0;                     /* second pair: +-0.4 % */
    uint32_t i2=scale(b0,-d/2+vib),i3=scale(b0,d+d/2+vib);
    /* Mod depth: phase offset per unit output, up to ~2 cycles at full scale. */
    unsigned m=p->mod>127?127:p->mod;
    uint32_t D=m*m*m/8u;                             /* cubic: clean to grit by ~60, chaos near 127 */
    if(h)D=(uint32_t)((int32_t)D+(int32_t)(D>>9)*(l>>7));   /* +-50 % with the LFO */
    uint32_t Dx=D>>1,Dy=D>>2;                        /* pair B -> A, pair A -> B */
    unsigned sh=p->sharp>127?127:p->sharp;
    int32_t g=(int32_t)(lyra_sharp_gain[sh]>>4);      /* Q8, 256 = triangle */
    uint32_t ph0=s->ph[0],ph1=s->ph[1],ph2=s->ph[2],ph3=s->ph[3];
    int32_t y0=s->y[0],y1=s->y[1],y2=s->y[2],y3=s->y[3],dc_x=s->dc_x,dc_y=s->dc_y;
    for(unsigned i=0;i<32;i++) {
        int32_t pa=(y0+y1)>>1,pb=(y2+y3)>>1,t,w;
        ph0+=i0;ph1+=i1;ph2+=i2;ph3+=i3;
#define OSC(ph,pm,y) t=(int32_t)((ph)+(pm));w=((t^(t>>31))>>15)-32768;y=clamp15((w*g)>>8);
        OSC(ph0,(uint32_t)y1*D+(uint32_t)pb*Dx,y0)
        OSC(ph1,(uint32_t)y0*D+(uint32_t)pb*Dx,y1)
        OSC(ph2,(uint32_t)y3*D+(uint32_t)pa*Dy,y2)
        OSC(ph3,(uint32_t)y2*D+(uint32_t)pa*Dy,y3)
#undef OSC
        /* DC blocker (~15 Hz): feedback modulation can lean the waveform. */
        int32_t x=y0+y1+y2+y3;x-=x>>2;                /* 3/4: headroom for the blocker's overshoot */
        dc_y=x-dc_x+dc_y-(dc_y>>9);dc_x=x;
        out[i]=clamp17(dc_y)*8192;                   /* four full-scale outputs: half of Q31 */
    }
    s->ph[0]=ph0;s->ph[1]=ph1;s->ph[2]=ph2;s->ph[3]=ph3;
    s->y[0]=y0;s->y[1]=y1;s->y[2]=y2;s->y[3]=y3;s->dc_x=dc_x;s->dc_y=dc_y;
}

#ifdef __m68k__
/* ---- Model:Cycles voice-loop glue (same contract as MACRO, notes/43) ------
 * update(pmod, v, p) then render(out, v) once per 32-frame block. pmod: trig
 * note << 16; v: the track's voice (0x31c bytes); p: track parameters (8.8
 * words: +0x14 PITCH, +0x16 COLOR, +0x18 SHAPE, +0x1a SWEEP, +0x1c CONTOUR,
 * +0x1e PUNCH, +0x22 FINE, +0x24 DECAY); v+0x34 / v+0x38: trig this block /
 * last block. State is ours, per track, initialised at the first block after
 * the machine changed (mark in v+0x2c, cleared by the voice reset 0x400a7ab8). */
#define CYC_VOICE0  0x42308828u
#define CYC_VSTRIDE 0x31cu
#define MARK_OFF    0x2c
#define MARK        0x4c595231u                      /* 'LYR1' */
#define AMP_ENV(v)  ((void (*)(char *))0x400a9252u)(v)
#define VCA(o,v)    ((void (*)(int32_t *,char *))0x400a9430u)(o,v)
#define PUNCH(o,v)  ((void (*)(int32_t *,char *))0x400a967au)(o,v)
#define LUT_DECAY   ((const int32_t *)0x4011d84cu)
#define V32(o)      (*(int32_t *)(v+(o)))
#define P16(o)      (*(const int16_t *)(p+(o)))
#define IDLE_LEVEL  (1<<14)

static lyra_state tracks[6];
static lyra_params params[6];
static unsigned index_of(const char *v){unsigned t=((uint32_t)v-CYC_VOICE0)/CYC_VSTRIDE;return t<6?t:0;}
static unsigned knob(int16_t w){int32_t x=w>>8;return x<0?0u:x>127?127u:(unsigned)x;}
static uint32_t mag(int32_t x){return x<0?0u-(uint32_t)x:(uint32_t)x;}

void lyra_update(int32_t pmod,char *v,const char *p) {
    unsigned t=index_of(v);
    if(V32(MARK_OFF)!=(int32_t)MARK) {
        lyra_state *s=&tracks[t];
        for(unsigned i=0;i<4;i++){s->ph[i]=0;s->y[i]=0;}
        s->lfo=0;s->dc_x=0;s->dc_y=0;
        V32(MARK_OFF)=(int32_t)MARK;
    }
    /* Stock amp chain, set up as TONE (0x400aa7b8) / MACRO. */
    int32_t dec=(int8_t)p[0x24];
    dec=dec<0?0:dec>127?127:dec;
    V32(0x250)=LUT_DECAY[dec];
    if(V32(0x38)) {
        V32(0x274)=V32(0x278)=0x012c0f26;
        V32(0x280)=V32(0x284)=0x001f7c5c;
        V32(0x290)=V32(0x294)=0x7fffffff;
        V32(0x27c)=(int32_t)0x82581e4bu;
        V32(0x288)=(int32_t)0x803ef8b8u;
        if(P16(0x1e)){V32(0x29c)=0x20000000;V32(0x2a0)=0x60000000;V32(0x2a4)=0x20000000;V32(0x48)=1;}
        else{V32(0x29c)=0x7fffffff;V32(0x2a0)=0x20000000;V32(0x2a4)=0x20000000;V32(0x48)=0;}
    }
    lyra_params *q=&params[t];
    int32_t n=((int32_t)P16(0x14)<<8)+(((int32_t)P16(0x22)-0x4000)<<3)-(64<<16)+pmod;
    q->note=n<0?0:n>(127<<16)?127<<16:n;
    q->spread=(uint8_t)knob(P16(0x16));q->sharp=(uint8_t)knob(P16(0x18));
    q->mod=(uint8_t)knob(P16(0x1a));q->hyper=(uint8_t)knob(P16(0x1c));
}

void lyra_render(int32_t *out,char *v) {
    if(!V32(0x34)&&!V32(0x38)&&mag(V32(0x230))<IDLE_LEVEL&&mag(V32(0x234))<IDLE_LEVEL) {
        for(unsigned i=0;i<32;i++)out[i]=0;          /* silent voice: not computed */
        return;
    }
    unsigned t=index_of(v);
    lyra_block(&tracks[t],&params[t],out);
    AMP_ENV(v);
    VCA(out,v);
    PUNCH(out,v);
}
#endif

#ifdef __m68k__
/* Knob names for machine 5's descriptors (long names shown while turning). */
const char lyra_name_spread[]="Spread",lyra_name_sharp[]="Sharpness",lyra_name_mod[]="Cross Mod",lyra_name_hyper[]="Hyper LFO";
const char lyra_short_spread[]="SPRD",lyra_short_sharp[]="SHRP",lyra_short_mod[]="XMOD",lyra_short_hyper[]="HYPR";
#endif
