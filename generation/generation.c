#include "generation.h"
#include <stddef.h>
extern void *memset(void *,int,size_t);
static uint32_t next(uint32_t *s){ uint32_t x=*s; x^=x<<13;x^=x>>17;x^=x<<5;return *s=x; }
static unsigned draw(uint32_t *s,unsigned n){ return next(s)%n; }
static unsigned range(uint32_t *s,unsigned a,unsigned b){return a+draw(s,b-a+1);}
/* murmur3 finalizer: neighbouring request seeds give unrelated streams. */
static uint32_t mix(uint32_t x){x^=x>>16;x*=0x85ebca6bu;x^=x>>13;x*=0xc2b2ae35u;x^=x>>16;return x?x:0x6d2b79f5u;}
int mc_density_active(unsigned g){return g==MC_PHRASE||g==MC_PATTERN;}
int mc_brutality_active(unsigned g,unsigned p){return p!=MC_MELODIC&&(g<MC_PHRASE||p==MC_RHYTHMIC||p==MC_FREE);}
/* ---- Sound archetypes ----------------------------------------------------
   Each seed picks one archetype (independent of Brutality), then draws every
   control inside the archetype's ranges. Brutality slides each COLOR, SHAPE,
   SWEEP, CONTOUR range from its b=0 bounds to its b=100 bounds and raises
   Volume+Dist, PUNCH odds and LFO depth. The number of draws never depends on
   Brutality, so a seed keeps its machine, pitch and decay at any setting.
   Macro meanings follow the OS1.13 manual (KICK/SNARE/METAL SHAPE and TONE
   SWEEP are feedback; TONE SHAPE127+SWEEP127+CONTOUR0 is noise; LYRA (machine 5,
   in place of CHORD) see its archetypes; MACRO COLOR=timbre, SHAPE=model, SWEEP=colour,
   CONTOUR=envelope to timbre). */
enum { PA=1,PR=2,PM=4,PF=8 };
enum { RAN=1,RIM=2,RDE=4,RTX=8,RPI=16,RRE=32,RANY=63 };
enum { LNONE=1,LSH=2,LAU=4,LEV=8,LDR=16,LCH=32 };
typedef struct { uint8_t machine,paths,roles,models,weight,pitch[2],decay[2],macro[4][4],punch[2],gate,vol[4],lfo; } archetype;
static const archetype archetypes[]={
 /* KICK: pitch64..76, PUNCH on, decay>=40 are kept for every kick. */
 {0,PR,RAN,0,6,{64,76},{50,78},{{10,45,30,80},{30,70,60,105},{30,80,50,110},{20,60,40,100}},{100,100},0,{96,112,112,127},LNONE|LEV},        /* punch kick */
 {0,PA|PR|PF,RAN,0,4,{64,76},{45,85},{{20,60,50,110},{40,80,90,127},{30,70,40,90},{30,70,70,127}},{100,100},0,{78,96,105,127},LNONE|LEV|LSH},   /* overdriven kick */
 {0,PA|PR|PF,RAN,0,2,{64,76},{40,70},{{40,100,80,127},{100,127,120,127},{20,60,30,90},{80,127,110,127}},{100,100},0,{80,100,110,127},LSH|LEV},   /* noise kick */
 {0,PA|PF,RAN,0,3,{64,72},{75,120},{{0,25,10,60},{0,40,20,80},{60,110,80,127},{0,40,20,90}},{100,100},0,{75,95,100,122},LNONE|LEV|LDR},     /* long boom */
 {0,PA|PF,RAN,0,2,{66,76},{40,55},{{60,127,90,127},{20,60,60,127},{80,127,100,127},{60,127,90,127}},{100,100},0,{80,100,108,127},LNONE|LSH}, /* hard click */
 {0,PA|PF,RANY,0,2,{64,76},{40,110},{{0,127,0,127},{0,127,60,127},{0,127,40,127},{0,127,60,127}},{100,100},0,{70,100,100,127},LSH|LAU|LEV|LCH}, /* mangled kick */
 /* SNARE: COLOR inharmonicity, SHAPE mod+feedback, SWEEP pitch sweep. */
 {1,PA|PR|PF,RIM|RRE,0,4,{56,80},{22,55},{{70,115,100,127},{85,127,110,127},{10,40,20,50},{50,110,90,127}},{60,100},0,{70,92,100,127},LNONE|LSH|LAU}, /* noise snare */
 {1,PR|PF,RIM|RDE,0,3,{64,84},{8,28},{{20,80,60,127},{40,90,80,127},{10,40,20,45},{70,127,100,127}},{80,100},0,{72,95,105,127},LNONE|LSH},        /* dry crack */
 {1,PA|PR|PF,RRE|RPI,0,2,{46,72},{30,80},{{30,100,60,127},{20,70,60,127},{10,40,20,40},{20,80,60,127}},{40,80},0,{65,85,90,115},LNONE|LEV|LSH}, /* inharmonic tom */
 /* METAL: COLOR/SWEEP tune offsets, SHAPE mod+feedback; high PITCH breaks out. */
 {2,PA|PR|PF,RDE|RTX,0,4,{78,88},{4,26},{{0,127,0,127},{70,120,100,127},{0,127,0,127},{0,60,20,127}},{0,30},0,{64,84,80,110},LNONE|LSH|LAU},       /* out-of-bounds hat */
 {2,PA|PR|PF,RIM|RRE|RTX,0,3,{60,88},{18,60},{{0,127,0,127},{60,115,95,127},{0,127,0,127},{40,110,80,127}},{30,70},0,{66,88,95,122},LNONE|LSH|LEV}, /* clang */
 {2,PA|PM|PF,RPI|RTX|RRE,0,3,{40,76},{50,120},{{0,127,0,127},{5,45,20,80},{0,127,0,127},{0,50,10,90}},{0,0},20,{60,80,75,100},LDR|LEV|LSH|LCH}, /* detuned ring */
 /* PERC: low SHAPE with moving COLOR rings inharmonically. */
 {3,PA|PR|PM|PF,RRE|RPI|RDE,0,3,{50,84},{25,80},{{40,127,70,127},{0,25,5,40},{5,35,10,40},{20,90,50,127}},{20,40},0,{62,82,80,110},LNONE|LSH|LEV}, /* signpost */
 {3,PR|PF,RDE,0,2,{74,88},{0,12},{{50,127,50,127},{60,127,100,127},{10,40,20,40},{80,127,110,127}},{20,60},0,{66,90,90,120},LNONE|LSH},          /* click */
 /* TONE: COLOR ratio, SHAPE mod amount, SWEEP feedback, CONTOUR mod envelope. */
 {4,PA|PR|PF,RIM|RDE|RTX|RRE,0,4,{52,84},{12,50},{{0,18,0,10},{115,127,122,127},{110,127,120,127},{0,25,0,50}},{40,90},0,{70,90,100,127},LNONE|LSH|LAU|LCH}, /* noise burst */
 {4,PR|PF,RDE,0,3,{70,88},{6,24},{{2,14,0,14},{118,127,122,127},{118,127,122,127},{35,70,50,100}},{0,30},0,{64,82,80,110},LNONE|LSH},             /* tone hat */
 {4,PA|PR|PM|PF,RPI|RRE,0,4,{40,64},{35,100},{{8,40,8,60},{0,30,20,70},{85,127,105,127},{0,12,0,40}},{0,40},30,{66,86,90,118},LNONE|LDR|LEV},      /* nasty bass */
 {4,PA|PR|PF,RTX|RRE|RPI,0,3,{60,88},{30,110},{{0,127,0,127},{90,127,110,127},{100,127,115,127},{60,127,90,127}},{20,60},25,{66,90,95,125},LAU|LSH|LDR|LEV|LCH}, /* feedback scream */
 {4,PA|PM|PF,RPI|RRE,0,3,{52,84},{25,90},{{0,127,0,127},{30,85,60,110},{0,60,20,90},{50,127,80,127}},{0,20},0,{60,78,70,95},LNONE|LDR|LEV},         /* FM bell */
 {4,PA|PM|PF,RPI,0,3,{40,60},{60,127},{{10,60,10,80},{40,90,70,120},{40,100,70,127},{0,60,0,90}},{0,20},50,{64,84,85,110},LDR|LAU|LEV},             /* growl */
 /* LYRA (machine 5, replaces CHORD): COLOR spread (0..63 detune, 64..127 the
    second pair at an interval), SHAPE sharpness (triangle..square), SWEEP
    cross-modulation (clean..chaos), CONTOUR hyper-LFO rate (0 off). */
 /* LYRA is a texture voice, never percussive: long decays or GATE, texture role only. */
 {5,PA|PR|PM|PF,RTX,0,4,{40,76},{70,127},{{10,63,30,127},{0,60,40,127},{0,40,30,90},{0,60,20,100}},{0,0},50,{60,80,80,110},LDR|LEV|LSH},        /* drone swarm */
 {5,PA|PR|PF,RTX,0,3,{40,64},{60,120},{{0,40,20,80},{40,127,80,127},{40,80,70,127},{60,127,90,127}},{0,0},40,{66,88,95,125},LAU|LSH|LEV|LDR}, /* growl texture */
 {5,PA|PR|PM|PF,RTX,0,3,{52,80},{60,110},{{64,127,64,127},{0,40,10,70},{0,25,10,50},{0,40,0,70}},{0,0},40,{60,80,75,105},LNONE|LDR|LEV},       /* interval haze */
 {5,PA|PR|PF,RTX,0,3,{40,72},{80,127},{{20,90,40,127},{60,127,100,127},{80,110,100,127},{20,90,60,127}},{0,0},60,{64,86,90,120},LDR|LSH},       /* noise cloud */
 /* MACRO (Braids models, see model sets). */
 {MC_MACRO,PA|PF,RAN,1,3,{64,76},{36,80},{{40,90,70,127},{0,0,0,0},{30,90,60,127},{20,80,60,127}},{100,100},0,{78,96,105,127},LNONE|LSH|LEV},
 {MC_MACRO,PA|PR|PF,RIM|RRE,2,3,{56,80},{18,55},{{40,110,80,127},{0,0,0,0},{30,110,70,127},{20,90,60,127}},{60,100},0,{70,92,100,127},LNONE|LSH|LAU},
 {MC_MACRO,PA|PR|PF,RDE|RTX,3,3,{66,88},{18,30},{{30,127,60,127},{0,0,0,0},{30,127,60,127},{0,80,40,127}},{0,40},0,{64,84,80,110},LNONE|LSH},
 {MC_MACRO,PA|PR|PF,RTX|RRE|RIM,4,4,{48,84},{20,110},{{0,127,40,127},{0,0,0,0},{0,127,40,127},{0,100,40,127}},{20,60},20,{66,90,95,125},LSH|LAU|LEV|LDR|LCH},
 {MC_MACRO,PA|PM|PF,RPI|RRE,5,4,{40,80},{25,110},{{0,127,0,127},{0,0,0,0},{0,127,0,127},{0,100,20,127}},{0,20},25,{62,82,80,105},LNONE|LDR|LEV|LSH},
 {MC_MACRO,PA|PF,RANY,6,3,{40,88},{18,127},{{0,127,40,127},{0,0,0,0},{0,127,40,127},{0,127,30,127}},{20,60},25,{62,90,90,125},LNONE|LSH|LAU|LEV|LDR|LCH},
};
#define ARCHETYPES (sizeof archetypes/sizeof archetypes[0])
/* MACRO model sets: kick; snare/struck/chaotic FM/twin-peaks noise; cymbal and
   noises; noisy/aggressive; tonal (never drums 32..36 or noises); all47. */
static const uint64_t model_sets[7]={0,UINT64_C(1)<<34,
 (UINT64_C(1)<<36)|(UINT64_C(1)<<33)|(UINT64_C(1)<<27)|(UINT64_C(1)<<42),
 (UINT64_C(1)<<35)|(UINT64_C(1)<<43)|(UINT64_C(1)<<41)|(UINT64_C(1)<<45),
 (UINT64_C(0x3f)<<41)|(UINT64_C(1)<<16)|(UINT64_C(1)<<13)|(UINT64_C(3)<<26)|(UINT64_C(1)<<38)|(UINT64_C(1)<<15),
 (UINT64_C(0xffffffff))|(UINT64_C(15)<<37),(UINT64_C(1)<<47)-1};
static unsigned bits(uint64_t v){unsigned n=0;while(v){n+=(unsigned)(v&1);v>>=1;}return n;}
static unsigned nth_bit(uint64_t v,unsigned k){unsigned i;for(i=0;i<64;i++)if(v&(UINT64_C(1)<<i)){if(!k)return i;k--;}return 0;}
static int lerp(int a,int b,unsigned t){return a+(b-a)*(int)t/100;}
/* u in 0..1023 maps onto the Brutality-interpolated range. */
static uint8_t pick(unsigned u,const uint8_t r[4],unsigned b){
 int lo=lerp(r[0],r[2],b),hi=lerp(r[1],r[3],b),t;if(hi<lo){t=lo;lo=hi;hi=t;}
 return (uint8_t)(lo+(int)u*(hi-lo+1)/1024);
}
static int fits(const archetype *a,unsigned eligible,unsigned strict,unsigned path,unsigned role){
 if(!(eligible&(1u<<a->machine)))return 0;
 if(!strict)return 1;
 /* The unified rhythmic path borrows Abstract's textures for the texture role. */
 if(!(a->paths&(1u<<path))&&!(path==MC_RHYTHMIC&&role==MC_TEXTURE&&(a->paths&PA)))return 0;
 return path!=MC_RHYTHMIC||(a->roles&(1u<<role));
}
/* Weighted choice among archetypes for this path and role; if none fits the
   eligible machines, any archetype of an eligible machine. One draw always. */
static const archetype *choose_archetype(uint32_t *s,unsigned eligible,unsigned path,unsigned role){
 unsigned i,total,k=draw(s,1u<<20),pass;
 for(pass=0;pass<2;pass++){
  total=0;
  for(i=0;i<ARCHETYPES;i++)if(fits(&archetypes[i],eligible,!pass,path,role))total+=archetypes[i].weight;
  if(!total)continue;
  k%=total;
  for(i=0;i<ARCHETYPES;i++){
   if(!fits(&archetypes[i],eligible,!pass,path,role))continue;
   if(k<archetypes[i].weight)return &archetypes[i];
   k-=archetypes[i].weight;
  }
 }
 return 0;
}
/* Sounding note of the Rhythmic kick: the root inside F1..E2 (53..64), where
   the stock KICK keeps its low-end body (about 44..82 Hz; note 60 = 65 Hz). */
static unsigned kick_note(unsigned root){return 53u+(root+7u)%12u;}
static int is_kick(const mc_sound *o){return o->machine==0||(o->machine==MC_MACRO&&o->shape==34);}
/* LFO styles: per-trig random S&H, audio-rate modulation (about 0.5 kHz at the
   fastest setting), one-shot envelope sweep, slow drift, and rhythmic chop. */
static void lfo(mc_sound_ext *e,uint32_t *s,unsigned mask,const mc_sound *o,unsigned b){
 /* No pitch on envelope (pew), drift (siren) or sample-and-hold: pitch only
    moves at audio rate, where it is FM grit rather than a sweep. */
 static const uint8_t sh_dest[8]={11,12,13,14,11,13,18,22},au_dest[5]={10,17,11,12,13},ev_dest[7]={11,12,13,14,14,18,21},dr_dest[6]={11,12,13,14,22,12},ch_dest[3]={22,18,22};
 static const uint8_t au_wave[4]={1,0,2,6},dr_wave[3]={0,1,5};
 unsigned n=bits(mask),style=nth_bit(mask,draw(s,n?n:1)),u1=draw(s,1024),u2=draw(s,1024),u3=draw(s,1024),u4=draw(s,1024),sign=draw(s,2),m=0;
 e->lfo_speed=112;e->lfo_mult=4;e->lfo_fade=64;e->lfo_dest=0;e->lfo_wave=0;e->lfo_phase=0;e->lfo_mode=0;e->lfo_depth=64;
 switch(1u<<style){
 case LSH:e->lfo_wave=6;e->lfo_mode=1;e->lfo_speed=64;e->lfo_mult=0;e->lfo_dest=sh_dest[u1*8/1024];m=10+u2*20/1024+b*30/100;break;
 case LAU:e->lfo_wave=au_wave[u1*4/1024];e->lfo_mode=(uint8_t)(u3&1);e->lfo_speed=(uint8_t)(112+u3*16/1024);e->lfo_mult=(uint8_t)(u4<512?23:22);e->lfo_phase=(uint8_t)(u4&127);
  e->lfo_dest=au_dest[u2*5/1024];m=4+u2%23+b*36/100;
  if(u4%4==0){e->lfo_fade=(uint8_t)(64-8-u3%32);}
  break;
 case LEV:e->lfo_wave=4;e->lfo_mode=1;e->lfo_speed=(uint8_t)(72+u3*40/1024);e->lfo_mult=(uint8_t)(2+u4*5/1024);e->lfo_dest=ev_dest[u1*7/1024];m=16+u2*30/1024+b*17/100;break;
 case LDR:e->lfo_wave=dr_wave[u1*3/1024];e->lfo_speed=(uint8_t)(u3&1?66+u3%16:62-u3%16);e->lfo_mult=(uint8_t)(12+u4*4/1024);e->lfo_dest=dr_dest[u2*6/1024];m=5+u2%20+b*20/100;
  if(u4%3==0){e->lfo_fade=(uint8_t)(64-8-u1%32);}
  break;
 case LCH:e->lfo_wave=2;e->lfo_mode=1;e->lfo_speed=(uint8_t)(u3<512?96:80);e->lfo_mult=(uint8_t)(5+u4*4/1024);e->lfo_dest=ch_dest[u1*3/1024];m=30+u2*20/1024+b*13/100;sign=0;break;
 default:break;
 }
 if(m>63)m=63;
 if(e->lfo_dest)e->lfo_depth=(uint8_t)(sign?64+m:64-m);
 /* Kicks keep their tuning: no audio-rate pitch modulation on a kick. */
 if(is_kick(o)&&(1u<<style)==LAU&&(e->lfo_dest==10||e->lfo_dest==17))e->lfo_dest=11;
}
static void place(mc_sound_ext *e,uint32_t *s,unsigned path,unsigned generator,unsigned track,unsigned b,unsigned role){
 unsigned z1=draw(s,100),z2=draw(s,100),a1=draw(s,1024),a2=draw(s,1024),pz=draw(s,100),pu=draw(s,1024);
 e->delay=e->reverb=0;e->pan=64;
 if(path==MC_RHYTHMIC&&role==MC_TEXTURE){e->reverb=(uint8_t)(20+a1*90/1024);if(z2<60)e->delay=(uint8_t)(a2*100/1024);}   /* textures: Abstract sends */
 else if(path==MC_RHYTHMIC){if(z1<30+b/5)e->reverb=(uint8_t)(8+a1*50/1024);if(z2<25+b/5)e->delay=(uint8_t)(8+a2*60/1024);}
 else if(path==MC_ABSTRACT){e->reverb=(uint8_t)(20+a1*90/1024);if(z2<60)e->delay=(uint8_t)(a2*100/1024);}
 else if(path==MC_MELODIC){if(z1<50)e->reverb=(uint8_t)(a1*60/1024);if(z2<40)e->delay=(uint8_t)(a2*60/1024);}
 else{if(z1<50)e->reverb=(uint8_t)(a1*127/1024);if(z2<50)e->delay=(uint8_t)(a2*127/1024);}
 if(generator==MC_KIT&&track>=2&&pz<70)e->pan=(uint8_t)(30+pu*68/1024);
 else if(generator==MC_PATCH&&(path==MC_ABSTRACT||path==MC_FREE)&&pz<30)e->pan=(uint8_t)(24+pu*80/1024);
}
/* Rhythmic track 1 is the kick. Each seed picks one of seven characters,
   measured on the stock KICK voice (OS 1.13 DSP, note 57):
     DECAY sets the tail: 3 ~ 66 ms, 8 ~ 140, 12 ~ 205, 20 ~ 355, 30 ~ 590,
     45 ~ 1 s (to -40 dB); the low end holds even at the shortest settings.
     SWEEP sets the pitch drop: 0 none (pure tone), 60 ~ 300 -> 65 Hz,
     110+ a fast 700 Hz chirp. SHAPE (feedback) is clean below ~75 and turns
     into bright feedback noise around 80..108. COLOR adds upper harmonics.
     CONTOUR has no measurable effect on the stock kick.
   The character sets these and the distortion record; Brutality pushes each
   character harder within its own ranges (more drive, feedback, sweep), so a
   seed keeps its character at any Brutality. Always PUNCH, never GATE, dry
   and centred. Character draws use their own stream: other tracks keep theirs. */
enum { KICK_PUNCH,KICK_KNOCK,KICK_BOOM,KICK_RUMBLE,KICK_FEEDBACK,KICK_THUD,KICK_GABBER,KICK_CHARACTERS };
typedef struct { uint8_t weight,decay[2],sweep[2],shape[2],color[2],amount[4],drive[2],fold[2],tone[2],low[2],bias; } kick_character;
static const kick_character kick_characters[KICK_CHARACTERS]={
 /*            w  decay    sweep     shape     color     amount b0..b100  drive    fold     tone     low      bias */
 /* punch  */ {3,{6,16},  {50,85},  {30,60},  {20,60},  {30,55,70,110},{0,60},  {0,0},   {0,40},  {90,127},0},
 /* knock  */ {2,{2,7},   {85,127}, {40,70},  {50,100}, {40,70,80,120},{20,80}, {0,0},   {0,20},  {60,110},30},
 /* boom   */ {2,{22,40}, {12,35},  {0,30},   {0,25},   {12,30,35,70}, {0,30},  {0,0},   {20,70}, {100,127},0},
 /* rumble */ {3,{12,26}, {55,90},  {55,74},  {60,100}, {60,90,95,127},{40,100},{0,60},  {20,80}, {60,100},40},
 /* fdback */ {2,{6,20},  {60,110}, {80,108}, {60,110}, {50,80,90,127},{30,90}, {0,40},  {30,90}, {70,110},20},
 /* thud   */ {2,{10,22}, {0,18},   {10,40},  {30,80},  {25,45,55,95}, {0,50},  {0,0},   {40,100},{80,120},0},
 /* gabber */ {2,{10,24}, {70,110}, {45,65},  {40,80},  {90,110,115,127},{90,127},{30,80},{0,40}, {50,90},50},
};
static unsigned kick_pick(uint32_t *k,const uint8_t r[2]){return r[0]+draw(k,(unsigned)(r[1]-r[0])+1u);}
static void punch_kick(mc_sound *o,mc_sound_ext *e,unsigned b,uint32_t stream){
 uint32_t k=mix(stream^0x4b1c5eedu);
 unsigned total=0,i,c=0,pick;
 for(i=0;i<KICK_CHARACTERS;i++)total+=kick_characters[i].weight;
 pick=draw(&k,total);
 for(i=0;i<KICK_CHARACTERS;i++){if(pick<kick_characters[i].weight){c=i;break;}pick-=kick_characters[i].weight;}
 const kick_character *h=&kick_characters[c];
 /* Higher levels push the decay up within the character's range. */
 unsigned decay=h->decay[0]+(draw(&k,1024)*60u/100u+b*1024u*40u/10000u)*(unsigned)(h->decay[1]-h->decay[0])/1024u,sweep=kick_pick(&k,h->sweep),shape=kick_pick(&k,h->shape),color=kick_pick(&k,h->color);
 if(o->machine==0){
  /* Brutality: more sweep and harmonics; feedback rises but a clean
     character never crosses into the noise region (SHAPE 75). */
  sweep+=b*(127u-sweep)/400u;color+=b*(127u-color)/300u;
  {unsigned cap=h->shape[1]<75?74u:110u;shape+=b*20u/100u;if(shape>cap)shape=cap;}
  o->decay=(uint8_t)decay;o->sweep=(uint8_t)(sweep>127?127:sweep);o->shape=(uint8_t)shape;o->color=(uint8_t)(color>127?127:color);
  o->contour=(uint8_t)(20+draw(&k,60));
 }
 else{ /* MACRO bass drum (model 34): its own decay scale; short to medium. */
  o->decay=(uint8_t)(20+decay);draw(&k,60);
 }
 o->punch=1;o->gate=0;
 e->delay=0;e->reverb=0;e->pan=64;
 /* Distortion: amount between the character's b=0 and b=100 ranges. */
 {unsigned lo=(unsigned)lerp(h->amount[0],h->amount[2],b),hi=(unsigned)lerp(h->amount[1],h->amount[3],b);
  e->drive[0]=(uint8_t)(lo+draw(&k,1024)*(hi-lo+1)/1024);}   /* monotonic in Brutality */
 e->drive[1]=(uint8_t)kick_pick(&k,h->drive);
 e->drive[2]=(uint8_t)(h->fold[1]?kick_pick(&k,h->fold)*(20u+b*80u/100u)/100u:0u);
 e->drive[3]=(uint8_t)(draw(&k,100)<h->bias?draw(&k,90):0u);
 e->drive[4]=(uint8_t)kick_pick(&k,h->tone);
 e->drive[5]=(uint8_t)kick_pick(&k,h->low);
 if(e->lfo_wave!=4||(e->lfo_dest!=13&&e->lfo_dest!=14)){
  e->lfo_speed=112;e->lfo_mult=4;e->lfo_fade=64;e->lfo_dest=0;e->lfo_wave=0;e->lfo_phase=0;e->lfo_mode=0;e->lfo_depth=64;
 }
}
/* Distortion stage (firmware/drive.h). Amount grows with Brutality on
   Abstract, Rhythmic and Free; Melodic stays light and darker. Fold is the
   expensive feature and is used sparingly. Fixed draw count. */
static void drive_params(mc_sound_ext *e,uint32_t *s,unsigned path,unsigned b){
 unsigned u[7],i;for(i=0;i<7;i++)u[i]=draw(s,1024);
 uint8_t *d=e->drive;for(i=0;i<8;i++)d[i]=0;
 unsigned amount,fold_odds,bias_odds;
 switch(path){
 case MC_MELODIC:amount=u[0]%36;fold_odds=5;bias_odds=20;d[4]=(uint8_t)(30+u[4]%61);break;
 case MC_ABSTRACT:amount=20+u[0]%51+b*40/100;fold_odds=35;bias_odds=50;d[4]=(uint8_t)(u[4]%100);break;
 case MC_RHYTHMIC:amount=30+u[0]%31+b*60/100;fold_odds=10+b*30/100;bias_odds=40;d[4]=(uint8_t)(u[4]%71);break;
 default:amount=u[0]%128;fold_odds=30;bias_odds=50;d[4]=(uint8_t)(u[4]%128);break;
 }
 if(amount<1)amount=1;
 d[0]=(uint8_t)(amount>127?127:amount);
 {unsigned top=path==MC_MELODIC?29:40+b*87/100;d[1]=(uint8_t)(u[1]%(top+1));}
 if(u[2]%100<fold_odds)d[2]=(uint8_t)(20+u[2]%108);
 if(u[3]%100<bias_odds)d[3]=(uint8_t)(u[3]%128);
 if(u[5]%100<30)d[5]=(uint8_t)(u[5]%128);
 /* DJ filter: position stays off (it is the player's control); resonance by
    path, rising with Brutality on Rhythmic and Free. */
 d[6]=0;
 d[7]=(uint8_t)(path==MC_MELODIC?20+u[6]%41:path==MC_ABSTRACT?30+u[6]%71:path==MC_RHYTHMIC?(40+b*60/100+u[6]%20>127?127:40+b*60/100+u[6]%20):u[6]%128);
}
/* Stage cost in ColdFire instructions per sample, measured in the packaged
   firmware: 21 for the clipper, +9 tone, +9 bias, +18 low crossover or +24
   fold (which engages the crossover); drive alone is free. A Kit keeps all six tracks within 6 x 30 by dropping
   fold, then bias, then low from the costliest tracks; bypass costs ~0. */
static unsigned drive_cost(const uint8_t *d){
 if(!d[0])return 0;
 return 21+(d[4]?9:0)+(d[3]?9:0)+(d[2]?24:d[5]?18:0);
}
static void drive_budget(mc_sound_ext *x){
 for(;;){
  unsigned total=0,worst=0,i,k;for(i=0;i<6;i++){total+=drive_cost(x[i].drive);if(drive_cost(x[i].drive)>drive_cost(x[worst].drive))worst=i;}
  if(total<=180)return;
  uint8_t *d=x[worst].drive;
  for(k=0;k<3;k++){unsigned f=k==0?2:k==1?3:5;if(d[f]){d[f]=0;break;}}
  if(k==3)return;
 }
}
static void sound(mc_sound *o,mc_sound_ext *e,uint32_t *s,unsigned eligible,unsigned path,unsigned role,unsigned b,unsigned generator,unsigned track){
 const archetype *a=choose_archetype(s,eligible,path,role);
 if(!a)a=&archetypes[0];
 unsigned i,u[10];uint8_t value[4];
 for(i=0;i<10;i++)u[i]=draw(s,1024);
 o->machine=a->machine;
 o->pitch=(uint8_t)(a->pitch[0]+u[0]*(a->pitch[1]-a->pitch[0]+1)/1024);
 o->decay=(uint8_t)(a->decay[0]+u[1]*(a->decay[1]-a->decay[0]+1)/1024);
 for(i=0;i<4;i++)value[i]=pick(u[2+i],a->macro[i],b);
 o->color=value[0];o->shape=value[1];o->sweep=value[2];o->contour=value[3];
 if(a->machine==MC_MACRO){uint64_t set=model_sets[a->models];o->shape=(uint8_t)nth_bit(set,u[6]*bits(set)/1024);if(o->decay<18)o->decay=18;}
 /* A borrowed Abstract texture never brings a MACRO bass drum onto a Rhythmic non-kick track: granular cloud instead. */
 if(a->machine==MC_MACRO&&o->shape==34&&path==MC_RHYTHMIC&&role!=MC_ANCHOR)o->shape=44;
 o->punch=(uint8_t)(u[7]*100/1024<(unsigned)lerp(a->punch[0],a->punch[1],b));
 o->gate=(uint8_t)(u[8]*100/1024<a->gate);
 o->finetune=64;
 (void)u[9];
 /* Every generated track sits at full VOLUME+DIST (127); levels are trimmed by hand. */
 e->volume=127;
 if(is_kick(o)){
  /* Kicks always keep their protected tuning, punch and sustained body. */
  if(o->pitch<64||o->pitch>76)o->pitch=(uint8_t)(64+o->pitch%13);
  if(o->decay<(o->machine==0?40:36))o->decay=(uint8_t)((o->machine==0?40:36)+o->decay%40);
  o->punch=1;o->gate=0;
 }
 lfo(e,s,a->lfo,o,b);
 place(e,s,path,generator,track,b,role);
 drive_params(e,s,path,b);
}
/* Rhythmic Kit: track1 kick, track2 snare/impact, track3 hat/click, 4..6
   percussion and texture (TONE noise recipes qualify for 2..6; LYRA textures for track 5). */
static unsigned kit_roles(unsigned track){return track==0?0x01u|MC_MACRO_BIT:track==1?0x16u|MC_MACRO_BIT:track==2?0x1cu|MC_MACRO_BIT:track==4?0x3eu|MC_MACRO_BIT:0x1eu|MC_MACRO_BIT;}   /* LYRA (32): the texture track 5 only */
static int root_feasible(unsigned lower,unsigned upper,unsigned tonic,unsigned default_note){
 unsigned i;for(i=lower;i<=upper;i++){
  int effective=(int)i+(int)default_note-64;
  if(effective>=0&&effective<=127&&(unsigned)effective%12==tonic)return 1;
 }return 0;
}
static void tune_sound(mc_sound *o,unsigned tonic,unsigned default_note,unsigned path,unsigned lower_hint){
 unsigned i,lower=lower_hint>40?lower_hint:40,upper=88,best=o->pitch,best_distance=128,found=0;
 (void)path;
 if(o->machine==0||(o->machine==MC_MACRO&&o->shape==34)){lower=64;upper=76;}
 for(i=lower;i<=upper;i++){
  int effective=(int)i+(int)default_note-64;
  if(effective>=0&&effective<=127&&(unsigned)effective%12==tonic){
   unsigned distance=i>o->pitch?i-o->pitch:o->pitch-i;
   if(!found||distance<best_distance){best=i;best_distance=distance;found=1;}
  }
 }
 if(found)o->pitch=(uint8_t)best;
 o->finetune=64;
}
static void phrase(mc_phrase *o,uint32_t *s,unsigned path,unsigned role,unsigned density,unsigned brutality,unsigned length,unsigned root,unsigned phase,unsigned family,unsigned kick_floor,unsigned music_root,unsigned music_scale){
 unsigned n=1+density*(length-1)/100,i,j,pos,used[64]={0},order[64],score[64];
 /* Rhythmic textures (Abstract's legacy): few, long events. */
 if(path==MC_RHYTHMIC&&role==MC_TEXTURE)n=1+density*(length/4-1)/100;
 static const uint8_t scales[4][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,4,6,8,10,0},{0,2,3,7,8,0,0}};
 static const uint8_t count[4]={7,7,6,5};
 o->length=(uint8_t)length;o->role=(uint8_t)role;
 if(path==MC_RHYTHMIC){
  static const uint8_t motifs[6][6]={{0,3,6,10,12,15},{0,8,11,14,4,6},{0,5,7,10,15,3},{0,3,7,10,5,11},{0,1,6,7,12,13},{0,4,9,12,2,7}};
  unsigned cycle=family==3?12:family==5&&role==MC_DETAIL?15:family==5&&role==MC_RESPONSE?12:16;
  unsigned offset=role==MC_ANCHOR?0:role==MC_IMPACT?cycle/2:phase;
  for(i=0;i<length;i++){
   unsigned x=(i+cycle-offset%cycle)%cycle,rank=7;
   for(j=0;j<6;j++)if(x==motifs[family][j]%cycle){rank=j;break;}
   score[i]=rank*32+(i/cycle%2)*5+draw(s,5);order[i]=i;
  }
  /* Density selects a prefix of a full, stable ranked motif. Brutality applies
     a bijective temporal rotation to non-anchor voices; event count survives. */
  for(i=1;i<length;i++){unsigned at=i,idx=order[i];while(at&&score[order[at-1]]>score[idx]){order[at]=order[at-1];at--;}order[at]=idx;}
 }

 for(i=0;i<n;i++){
  if(path==MC_RHYTHMIC){pos=order[i];if(role!=MC_ANCHOR&&brutality>=40)pos=(pos+1+(brutality-40)/20)%length;}
  else if(path==MC_MELODIC)pos=(phase+i*length/n)%length;
  else pos=draw(s,length);
  if(path==MC_RHYTHMIC&&role==MC_TEXTURE)pos=(phase+i*length/n)%length;   /* textures: evenly spread */
  for(j=0;j<length&&used[pos];j++)pos=(pos+1)%length;
  used[pos]=1;o->step[pos].on=1;
  o->step[pos].note=(uint8_t)(path==MC_MELODIC?root+scales[music_scale][draw(s,count[music_scale])]+12*draw(s,2):path==MC_RHYTHMIC?root+draw(s,4):range(s,40,96));
  o->step[pos].velocity=(uint8_t)range(s,48,106);o->step[pos].length=(uint8_t)(path==MC_ABSTRACT?range(s,1,12):path==MC_MELODIC?range(s,1,6):role==MC_TEXTURE?range(s,4,12):1);
  if((path==MC_RHYTHMIC||path==MC_FREE)&&brutality){o->step[pos].velocity=(uint8_t)(o->step[pos].velocity+brutality*20/100);if(draw(s,100)<brutality/2)o->step[pos].note=(uint8_t)range(s,40,96);}
  if(kick_floor){if(o->step[pos].note<kick_floor)o->step[pos].note=(uint8_t)range(s,kick_floor,kick_floor+10>96?96:kick_floor+10);if(o->step[pos].velocity<96)o->step[pos].velocity=(uint8_t)range(s,96,127);}
  /* Quantize after pitch deviations and floor protection. Source pitch is
     accounted for by the caller in music_root. Fractional tuning is retained. */
  (void)mc_quantize_note(o->step[pos].note,music_root,music_scale,kick_floor?kick_floor:40,96,&o->step[pos].note);
 }
}
/* ---- Sequence articulation ------------------------------------------------
   Runs after events are placed, on its own random stream, so Brutality never
   moves, adds or removes events. Adds per-step parameter-lock lanes, lock-only
   (trigless) steps, microtiming, retrig ratchets/rolls and trig conditions.
   A lane is one sound word k modulated across the phrase by a shape:
   random per hit, ramp, accent (follows velocity), sparse spikes, or a short
   cycle of 2..5 values that drifts against the bar. Density sets how many
   lanes a phrase gets; Brutality (Rhythmic/Free) widens ranges to extremes and
   raises the odds of spikes, retrigs and timing displacement. A pattern saves
   at most MC_LOCK_PAIRS locked (track, slot) pairs; lanes come out of *budget. */
enum { SH_RANDOM,SH_RAMP,SH_ACCENT,SH_SPIKES,SH_CYCLE };
enum { K_LSPEED=1,K_LDEPTH=8,K_PITCH=10,K_COLOR=11,K_SHAPE=12,K_SWEEP=13,K_CONTOUR=14,K_PUNCH=15,K_DECAY=18,K_VOLUME=19,K_DELAY=20,K_REVERB=21,K_PAN=22 };
typedef struct { uint8_t k,shape,lo,hi,mid,up,period,cycles; uint8_t cycle[5]; int8_t span; } lane;
/* Candidate lane parameters and per-path weights: Abstract, Rhythmic, Melodic, Free. */
static const uint8_t lane_k[13]={K_COLOR,K_SHAPE,K_SWEEP,K_CONTOUR,K_DECAY,K_VOLUME,K_PUNCH,K_PITCH,K_PAN,K_DELAY,K_REVERB,K_LDEPTH,K_LSPEED};
/* Volume+Dist (index 5) is never locked: every track stays at 127. */
static const uint8_t lane_w[4][13]={{3,3,3,3,2,0,0,2,3,3,3,2,2},{3,3,3,3,3,0,2,2,1,1,1,1,1},{3,1,3,4,2,0,0,1,1,1,1,1,0},{2,2,2,2,2,0,2,2,2,2,2,2,2}};
static const uint8_t shape_mask[4]={(1<<SH_RAMP)|(1<<SH_RANDOM)|(1<<SH_CYCLE),(1<<SH_RANDOM)|(1<<SH_ACCENT)|(1<<SH_SPIKES)|(1<<SH_CYCLE),(1<<SH_RAMP)|(1<<SH_CYCLE)|(1<<SH_ACCENT),31};
static unsigned clamp127(int v){return v<0?0u:v>127?127u:(unsigned)v;}
static unsigned macro_model(uint32_t *a,unsigned path){
 static const uint8_t noisy[16]={41,42,43,44,45,46,16,13,26,27,38,15,33,35,36,28};
 if(path==MC_RHYTHMIC)return noisy[draw(a,16)];
 if(path==MC_MELODIC){unsigned m=draw(a,36);return m<32?m:m+5;}
 return draw(a,47);
}
/* Rhythmic locks keep the track recognisable: timbre lanes move around the
   track's own sound by a window that grows with Brutality, instead of
   anywhere in 0..127. A stock KICK's SHAPE stays on its side of the
   feedback-noise boundary (75..79). Decay may shorten a lot but grow only a
   little. The LFO lanes stay near "no change". */
static void keep_recognizable(lane *l,const mc_sound *o,unsigned b,unsigned machine){
 int centre,w,lo,hi;
 switch(l->k){
 case K_COLOR:centre=o->color;w=12+(int)(b*20/100);break;
 case K_SHAPE:centre=o->shape;w=8+(int)(b*14/100);break;
 case K_SWEEP:centre=o->sweep;w=10+(int)(b*18/100);break;
 case K_CONTOUR:centre=o->contour;w=12+(int)(b*20/100);break;
 case K_DECAY:centre=o->decay;w=0;break;
 case K_LDEPTH:centre=64;w=8+(int)(b*24/100);break;
 case K_LSPEED:centre=(l->lo+l->hi)/2;w=10+(int)(b*20/100);break;
 default:return;
 }
 if(l->k==K_DECAY){lo=centre-centre/2-2;hi=centre+(machine==0?2+(int)(b*6/100):4+(int)(b*12/100));}
 else{lo=centre-w;hi=centre+w;}
 if(lo<0)lo=0;
 if(hi>127)hi=127;
 if(l->k==K_DECAY&&lo<1)lo=1;
 if(l->k==K_SHAPE&&machine==0){if(centre<75){if(hi>74)hi=74;}else if(lo<80)lo=80;}
 if(hi<lo)hi=lo;
 l->lo=(uint8_t)lo;l->hi=(uint8_t)hi;
}
static void make_lane(lane *l,uint32_t *a,unsigned k,unsigned path,unsigned b,unsigned machine,const mc_sound *sound){
 unsigned c=draw(a,128),w,u=draw(a,1024),i,n=0,pick_shape,shapes=shape_mask[path];
 for(i=0;i<5;i++){n+=(shapes>>i)&1;}
 pick_shape=draw(a,n);
 for(i=0;i<5;i++)if((shapes>>i)&1){if(!pick_shape)break;pick_shape--;}
 l->k=(uint8_t)k;l->shape=(uint8_t)i;l->up=(uint8_t)(u&1);l->cycles=(uint8_t)(1+draw(a,3));l->period=(uint8_t)(2+draw(a,4));
 w=path==MC_ABSTRACT?60+u%68:path==MC_MELODIC?16+u%41:24+b*103/100;
 if(path==MC_FREE&&w<40)w=40;
 l->lo=(uint8_t)clamp127((int)c-(int)w/2);l->hi=(uint8_t)clamp127((int)l->lo+(int)w);
 if((path==MC_RHYTHMIC||path==MC_FREE)&&b>=60&&draw(a,100)<40){if(u&2)l->lo=0;else l->hi=127;}
 else draw(a,100);
 switch(k){
 case K_VOLUME:l->lo=(uint8_t)(path==MC_RHYTHMIC||path==MC_FREE?50+u%30:40+u%30);l->hi=(uint8_t)clamp127(l->lo+20+(int)(path==MC_RHYTHMIC||path==MC_FREE?b*60/100:20));break;
 case K_DELAY:case K_REVERB:l->lo=0;l->hi=(uint8_t)(60+u%68);if(l->shape==SH_RAMP||l->shape==SH_RANDOM)l->shape=SH_SPIKES;break;
 case K_PAN:if(path==MC_RHYTHMIC){l->lo=(uint8_t)(24+u%30);l->hi=(uint8_t)(128-l->lo);}else if(path==MC_MELODIC){l->lo=40;l->hi=88;}break;
 case K_DECAY:if(path==MC_RHYTHMIC){l->lo=(uint8_t)(4+u%20);l->hi=(uint8_t)clamp127(l->lo+20+(int)(b*80/100));}break;
 case K_LSPEED:case K_LDEPTH:break;
 case K_PUNCH:l->lo=0;l->hi=1;break;
  case K_PITCH:if(l->shape==SH_RAMP)l->shape=SH_RANDOM; /* no pitch ramps: they read as sirens */
  l->span=(int8_t)(path==MC_MELODIC?12:path==MC_ABSTRACT?5+u%20:path==MC_RHYTHMIC?2+b*10/100:3+b*21/100);break;
 case K_SHAPE:if(machine==MC_MACRO){l->lo=(uint8_t)(l->lo*46/127);l->hi=(uint8_t)(l->hi*46/127);} /* MACRO model */
  break;
 default:break;
 }
 if(path==MC_RHYTHMIC&&sound)keep_recognizable(l,sound,b,machine);
 else if(path==MC_RHYTHMIC&&(k==K_LDEPTH||k==K_LSPEED)){static const mc_sound neutral={0,64,64,64,64,64,0,0,64,64};keep_recognizable(l,&neutral,b,machine);}
 if(l->hi<l->lo)l->hi=l->lo;
 l->mid=(uint8_t)((l->lo+l->hi)/2);
 for(i=0;i<5;i++){l->cycle[i]=(uint8_t)(l->lo+draw(a,(unsigned)(l->hi-l->lo)+1));
  if(k==K_SHAPE&&machine==MC_MACRO)l->cycle[i]=(uint8_t)macro_model(a,path);else draw(a,2);}
}
/* Value of lane l at step pos; ordinal = index among locked steps, accent = loud hit. */
static unsigned lane_value(const lane *l,uint32_t *a,unsigned pos,unsigned length,unsigned ordinal,unsigned accent,unsigned b,unsigned path){
 unsigned span=(unsigned)(l->hi-l->lo),r=draw(a,1024),t;
 switch(l->shape){
 case SH_RAMP:t=(pos*l->cycles*1024/length)%1024;if(!l->up)t=1023-t;return l->lo+t*(span+1)/1024;
 case SH_ACCENT:return accent?l->hi:l->lo+r*(span/3+1)/1024;
 case SH_SPIKES:return r*100/1024<(path==MC_RHYTHMIC||path==MC_FREE?15+b/4:20)?(r&1?l->hi:l->lo):l->mid;
 case SH_CYCLE:return l->cycle[ordinal%l->period];
 default:return l->lo+r*(span+1)/1024;
 }
}
static void add_lock(mc_event *e,unsigned k,unsigned v){if(e->lock_count<MC_STEP_LOCKS){e->lock_k[e->lock_count]=(uint8_t)k;e->lock_v[e->lock_count]=(uint8_t)v;e->lock_count++;}}
/* Pitch lock as a scale-safe offset. Notes are quantized to music_root, which
   already folds in the sound's own PITCH (base); a lock replaces that PITCH, so
   lock = q - note + base keeps the played pitch in key. Lock stays in 40..88. */
static unsigned pitch_lock(unsigned note,int delta,unsigned root,unsigned scale,unsigned base,unsigned *value){
 int target=(int)note+delta;uint8_t q;
 if(target<40)target=40;
 if(target>96)target=96;
 if(!mc_quantize_note((unsigned)target,root,scale,40,96,&q))return 0;
 target=(int)q-(int)note+(int)base;
 if(target<40||target>88)return 0;
 *value=(unsigned)target;return 1;
}
typedef struct { unsigned path,role,density,brutality,machine,kick,music_root,music_scale,default_note,base_pitch,pan_owned; const mc_sound *sound; } art_ctx;
static void articulate(mc_phrase *o,uint32_t *s,const art_ctx *c,unsigned *budget){
 uint32_t a=mix(next(s)^0xa5172c3du);
 unsigned i,j,length=o->length,b=c->brutality,path=c->path,lanes,want,events=0,vsum=0,ordinal=0,weights[13],total,used=0;
 lane lane_set[MC_STEP_LOCKS];
 /* Rhythmic texture tracks keep Abstract's articulation: lock-only steps,
    floating timing, granular smears; their lock lanes stay Rhythmic. */
 unsigned feel=path==MC_RHYTHMIC&&c->role==MC_TEXTURE?(unsigned)MC_ABSTRACT:path;
 for(i=0;i<length;i++)if(o->step[i].on){events++;vsum+=o->step[i].velocity;}
 if(!events)return;
 /* Lanes: Density adds complexity; Brutality adds a lane on Rhythmic/Free. */
 want=(path==MC_MELODIC?1:2)+c->density*2/100+((path==MC_RHYTHMIC||path==MC_FREE)&&b>=50);
 /* A track whose panning is written by pan_image() keeps one lock per step for it. */
 if(want>MC_STEP_LOCKS-(c->pan_owned?1u:0u))want=MC_STEP_LOCKS-(c->pan_owned?1u:0u);
 lanes=want<*budget?want:*budget;*budget-=lanes;
 for(i=0;i<13;i++){
  unsigned k=lane_k[i],w=lane_w[path][i];
  if(k==K_PITCH&&c->kick)w=0;
  if(k==K_PUNCH&&path==MC_MELODIC)w=0;
  if(k==K_SHAPE&&c->machine==255)w=0;     /* unknown machine: SHAPE may be a MACRO model */
  if(k==K_PAN&&c->pan_owned)w=0;
  /* Rhythmic: never swap a MACRO track's model, and without the track's own
     sound known there is no window to keep timbre locks inside. */
  if(path==MC_RHYTHMIC&&k==K_SHAPE&&c->machine==MC_MACRO)w=0;
  if(path==MC_RHYTHMIC&&!c->sound&&(k==K_COLOR||k==K_SHAPE||k==K_SWEEP||k==K_CONTOUR||k==K_DECAY))w=0;
  weights[i]=w;
 }
 for(j=0;j<lanes;j++){
  unsigned pickw,k=K_COLOR;
  total=0;for(i=0;i<13;i++)if(!(used&(1u<<i)))total+=weights[i];
  pickw=total?draw(&a,total):0;
  for(i=0;i<13;i++){if(used&(1u<<i)||!weights[i])continue;if(pickw<weights[i]){k=lane_k[i];used|=1u<<i;break;}pickw-=weights[i];}
  if(!total){lanes=j;break;}
  make_lane(&lane_set[j],&a,k,path,b,c->machine,c->sound);
 }
 /* Lock-only steps animate parameters between hits (Abstract, Free, sometimes Melodic, high-Brutality Rhythmic). */
 {unsigned pct=feel==MC_ABSTRACT?15+draw(&a,31):feel==MC_FREE?draw(&a,26):feel==MC_MELODIC?draw(&a,11):(b>=50?draw(&a,9):0);
  if(lanes)for(i=0;i<length;i++)if(!o->step[i].on&&draw(&a,100)<pct)o->step[i].trigless=1;}
 for(i=0;i<length;i++){
  mc_event *e=&o->step[i];
  unsigned accent=e->on&&e->velocity*events>vsum*11/10,r1=draw(&a,100),r2=draw(&a,100),r3=draw(&a,1024),r4=draw(&a,1024);
  if(!e->on&&!e->trigless)continue;
  for(j=0;j<lanes;j++){
   const lane *l=&lane_set[j];unsigned v=lane_value(l,&a,i,length,ordinal,accent,b,path);
   if(l->k==K_PITCH){unsigned note=e->on?e->note:c->default_note,pv;int delta=(int)(r3%(2u*(unsigned)l->span+1))-l->span;
    if(l->shape==SH_CYCLE)delta=(int)v*2*l->span/127-l->span;
    if(pitch_lock(note,delta,c->music_root,c->music_scale,c->base_pitch,&pv))add_lock(e,K_PITCH,pv);}
   else if(l->k==K_PUNCH)add_lock(e,K_PUNCH,accent||v>=1?(r4&1)|accent:0);
   /* A Rhythmic kick stays centred on every quarter and near the centre between. */
   else if(l->k==K_PAN&&c->kick&&path==MC_RHYTHMIC)add_lock(e,K_PAN,i%4==0?64u:v<44?44u:v>84?84u:v);
   else add_lock(e,l->k,v);
  }
  if(e->trigless&&!e->lock_count){e->trigless=0;continue;}
  ordinal++;
  if(!e->on)continue;
  /* Microtiming: Abstract floats off the grid, Rhythmic drags and jitters the
     non-anchor voices with Brutality, Melodic breathes, Free anywhere. */
  if(feel==MC_ABSTRACT)e->micro=(int8_t)((int)(r3%47)-23);
  else if(feel==MC_RHYTHMIC&&!c->kick&&r1<20+b/2)e->micro=(int8_t)((int)(r3%(2u*(3u+b*15/100)+1))-(int)(3u+b*15/100));
  else if(feel==MC_MELODIC&&!c->kick&&r1<30)e->micro=(int8_t)((int)(r3%9)-4);
  else if(feel==MC_FREE&&r1<50)e->micro=(int8_t)((int)(r3%(2u*(4u+b*19/100)+1))-(int)(4u+b*19/100));
  /* Retrigs: noise rolls and ratchets on Rhythmic/Free (Brutality), granular
     smears on Abstract, rare ornaments on Melodic. */
  {unsigned odds=feel==MC_RHYTHMIC?(c->kick?(b>=60?(b-50)/5:0):5+b*40/100):feel==MC_FREE?5+b*45/100:feel==MC_ABSTRACT?20:6;
   if(r2<odds){
    if(feel==MC_ABSTRACT){e->retrig=(uint8_t)(1+r4%10);e->retrig_length=(uint8_t)(2+r3%11);e->retrig_fade=(int8_t)(-(int)(r4%120));}
    else if(feel==MC_MELODIC){e->retrig=(uint8_t)(7+r4%4);e->retrig_length=1;e->retrig_fade=(int8_t)(-(int)(r4%60));}
    else{unsigned fast=b>=50||path==MC_FREE;e->retrig=(uint8_t)(1+(fast?12+r4%5:8+r4%4));if(path==MC_FREE&&r4%5==0)e->retrig=(uint8_t)(1+r4%17);
     e->retrig_length=(uint8_t)(1+r3%(path==MC_FREE?4:2));e->retrig_fade=(int8_t)((int)(r3%181)-120);}
   }}
  /* Conditions keep loops evolving. Never on a kick; probabilities, A:B, FILL. */
  if(!c->kick){unsigned odds=feel==MC_RHYTHMIC?15+c->density/5:feel==MC_ABSTRACT?30:feel==MC_MELODIC?15:35,code=255;
   if(draw(&a,100)<odds){unsigned kind=draw(&a,10),x=draw(&a,1024);
    if(path==MC_FREE&&kind<3)code=x%64;
    else if(kind<6)code=path==MC_ABSTRACT?15+x%8:18+x%8;            /* 25..81% or 50..94% */
    else if(kind<9){static const uint8_t ab[6]={29,31,36,38,39,44};code=ab[x%6];} /* 1:2 1:3 2:4 4:4.. */
    else code=x&1?0:7;                                                /* FILL or !1ST */
    e->condition=(uint8_t)(code+1);}}
 }
}
/* Rhythmic Pattern stereo image. Every playing track gets a pan lock on each
   hit (one (track, Pan) pair each). The kick sits in the middle: every quarter
   (so every downbeat) is locked to the centre, and some seeds let the hits in
   between alternate a little left and right. The other voices get home
   positions spread evenly and symmetrically across the field, in a seeded
   order, so the image stays balanced; each moves around its own home: static,
   ping-pong, scatter, accent flips to the mirrored side, or a sweep through the
   bar. Brutality widens the field and the motion. Own random stream: the
   rest of the pattern does not change with it. */
static void pan_image(mc_plan *p,const unsigned *tracks,unsigned active,uint32_t seed,unsigned b){
 uint32_t a=mix(seed^0x5bd1e995u);
 unsigned n=active-1,i,j,order[5]={0,1,2,3,4},width=28+draw(&a,17)+b*12/100;
 if(width>56)width=56;
 for(i=n;i>1;i--){unsigned t,k=draw(&a,i);t=order[i-1];order[i-1]=order[k];order[k]=t;}
 /* Kick: centred, with optional small alternation on off-quarter hits. */
 {mc_phrase *o=&p->phrase[0];unsigned swing=draw(&a,100)<50?8+draw(&a,13)+b/10:0,flip=draw(&a,2),ord=0;
  for(i=0;i<o->length;i++){mc_event *e=&o->step[i];if(!e->on)continue;
   unsigned v=64;if(swing&&i%4){v=(ord++&1)^flip?64+swing:64-swing;}
   add_lock(e,K_PAN,v);}}
 for(j=0;j<n;j++){
  mc_phrase *o=&p->phrase[tracks[j+1]];
  /* Slot order[j] of n, evenly from -width to +width, jittered by up to 4. */
  int home=n>1?64-(int)width+(int)(2*width*order[j]/(n-1)):64+((draw(&a,2))?(int)width:-(int)width);
  home+=(int)draw(&a,9)-4;
  unsigned style=draw(&a,11),d=10+draw(&a,15)+b*16/100,flip=draw(&a,2),ord=0,events=0,vsum=0;
  for(i=0;i<o->length;i++)if(o->step[i].on){events++;vsum+=o->step[i].velocity;}
  for(i=0;i<o->length;i++){mc_event *e=&o->step[i];if(!e->on)continue;
   int v=home;
   if(style<2){}                                                     /* static */
   else if(style<5)v=(ord&1)^flip?home+(int)d:home-(int)d;           /* ping-pong */
   else if(style<7)v=home+(int)draw(&a,2*d+1)-(int)d;                /* scatter */
   else if(style<9){if(e->velocity*events>vsum*11/10)v=128-home;}    /* accents flip sides */
   else{unsigned t=(i*2048/o->length)%2048;t=t<1024?t:2047-t;v=home-(int)d+(int)(2*d*t/1023);} /* sweep */
   ord++;
   add_lock(e,K_PAN,clamp127(v));
  }
 }
}
int mc_generate(const mc_request *q,mc_plan *out){
 mc_request captured;mc_plan *p=out;uint32_t s;unsigned i,j,tracks[6]={0,1,2,3,4,5},roles[6]={0,1,2,3,4,5},root,family,length,b,d,active,budget;uint8_t tonic,music_scale;unsigned kick_root;
 if(!q||!out)return 0;
 captured=*q;q=&captured;
 if(q->generator>3||q->path>3||q->target>=6||q->density>100||q->brutality>100||!(q->machine_mask&((1u<<MC_MACHINES)-1u))||q->machine_valid_mask&~63||q->source_sound_valid_mask&~63||q->note_valid_mask&~63||q->phrase_length>64)return 0;
 if(!mc_music_unpack(q->music,&tonic,&music_scale))return 0;
 /* Rhythmic track 1 kick: the chosen root, or C when no scale was chosen. */
 kick_root=q->music?tonic:0;
 for(i=0;i<6;i++)if((q->note_valid_mask&(1u<<i))&&q->default_note[i]>127)return 0;
 for(i=0;i<6;i++)if((q->machine_valid_mask&(1u<<i))&&(q->current_machine[i]>=MC_MACHINES||!(q->machine_mask&(1u<<q->current_machine[i]))))return 0;
 for(i=0;i<6;i++)if(q->source_sound_valid_mask&(1u<<i)){
  const mc_sound *a=&q->current_sound[i];
  if(!(q->machine_valid_mask&(1u<<i))||a->machine!=q->current_machine[i]||a->pitch<40||a->pitch>88||a->color>127||a->shape>127||a->sweep>127||a->contour>127||a->punch>1||a->gate>1||a->finetune>127||a->decay>127||(a->machine==MC_MACRO&&a->shape>46))return 0;
 }
 if(q->generator<MC_PHRASE){unsigned wanted=q->generator==MC_PATCH?1u<<q->target:63;for(i=0;i<6;i++)if(wanted&(1u<<i)){
  unsigned eligible=q->machine_mask;
  if(q->generator==MC_KIT&&q->path==MC_RHYTHMIC)eligible&=kit_roles(i);
  if(!eligible)return 0;
  /* Conservatively cover every eligible profile before touching caller scratch.
     Extremely high custom pad notes can make protected kick/hat roots impossible. */
  {unsigned note=(q->note_valid_mask&(1u<<i))?q->default_note[i]:60,lower=40;
   if(q->generator==MC_KIT&&q->path==MC_RHYTHMIC&&i==2)lower=70;
   unsigned root_here=q->path==MC_RHYTHMIC&&i==0?kick_root:tonic;
   if(!root_feasible(lower,88,root_here,note))return 0;
   if((eligible&1u)||((eligible&MC_MACRO_BIT)&&q->path!=MC_MELODIC&&!(q->generator==MC_KIT&&q->path==MC_RHYTHMIC&&(i==1||i==2)))){
    if(!root_feasible(64,76,root_here,note))return 0;
   }
  }
 }}
 memset(p,0,sizeof *p);s=mix(q->seed);root=range(&s,40,60);{unsigned best=40,distance=128;for(i=40;i<=60;i++)if(i%12==tonic){unsigned delta=i>root?i-root:root-i;if(delta<distance){best=i;distance=delta;}}root=best;}family=draw(&s,6);length=(unsigned[]){16,32,48,64}[draw(&s,4)];if(q->generator==MC_PHRASE&&q->phrase_length)length=q->phrase_length;
 b=mc_brutality_active(q->generator,q->path)?q->brutality:0;d=mc_density_active(q->generator)?q->density:0;
 for(i=5;i>0;i--){unsigned t;j=draw(&s,i+1);t=tracks[i];tracks[i]=tracks[j];tracks[j]=t;t=roles[i];roles[i]=roles[j];roles[j]=t;}
 if(q->generator>=MC_PHRASE){static const unsigned from_machine[MC_MACHINES]={MC_ANCHOR,MC_IMPACT,MC_DETAIL,MC_RESPONSE,MC_PITCHED,MC_TEXTURE,MC_PITCHED};for(i=0;i<6;i++)if(q->machine_valid_mask&(1u<<i)){roles[i]=from_machine[q->current_machine[i]];if(q->current_machine[i]==MC_MACRO&&(q->source_sound_valid_mask&(1u<<i))){unsigned model=q->current_sound[i].shape;roles[i]=model==34?MC_ANCHOR:model==33||model==36?MC_IMPACT:model==35?MC_DETAIL:model>=41?MC_TEXTURE:MC_PITCHED;}}}
 /* Rhythmic: track 1 is always the kick, for every generator. Patch, Kit and
    Pattern use the full six-track role layout; Phrase keeps roles inferred
    from the other tracks' machines. */
 if(q->path==MC_RHYTHMIC)roles[0]=MC_ANCHOR;
 if(q->path==MC_RHYTHMIC&&q->generator!=MC_PHRASE){
  roles[0]=MC_ANCHOR;roles[1]=MC_IMPACT;roles[2]=MC_DETAIL;
  roles[3]=MC_RESPONSE;roles[4]=MC_TEXTURE;roles[5]=MC_IMPACT;
  /* A LYRA track is a texture wherever it sits. */
  if(q->generator>=MC_PHRASE)for(i=1;i<6;i++)if((q->machine_valid_mask&(1u<<i))&&q->current_machine[i]==5)roles[i]=MC_TEXTURE;
  if(q->generator==MC_PATTERN){
   /* The kick (track 1) always plays; the other voices come in the seed's
      shuffled order, so any combination of tracks 2..6 can join it. */
   for(i=0;tracks[i]!=0;i++){}
   for(;i>0;i--)tracks[i]=tracks[i-1];
   tracks[0]=0;
  }
 }
 if(q->generator==MC_PATCH||q->generator==MC_KIT){p->sound_mask=q->generator==MC_PATCH?(uint8_t)(1u<<q->target):63;for(i=0;i<6;i++)if(p->sound_mask&(1u<<i)){unsigned eligible=q->machine_mask;if(q->generator==MC_KIT&&q->path==MC_RHYTHMIC)eligible&=kit_roles(i);
 sound(&p->sound[i],&p->extra[i],&s,eligible,q->path,roles[i],b,q->generator,i);
 if(q->path==MC_RHYTHMIC&&i==0&&is_kick(&p->sound[i]))punch_kick(&p->sound[i],&p->extra[i],b,s);
 /* The rhythmic Kit hat stays short whatever detail archetype was drawn. */
 if(q->generator==MC_KIT&&q->path==MC_RHYTHMIC&&i==2&&p->sound[i].decay>30)p->sound[i].decay=(uint8_t)(6+p->sound[i].decay%25);
tune_sound(&p->sound[i],q->path==MC_RHYTHMIC&&i==0?kick_root:tonic,(q->note_valid_mask&(1u<<i))?q->default_note[i]:60,q->path,q->generator==MC_KIT&&q->path==MC_RHYTHMIC&&i==2?70:40);
 if(q->path==MC_RHYTHMIC&&i==0&&p->sound[i].machine==0){
  /* The default note plays the kick note: PITCH = kick note - default + 64. */
  int dn=(q->note_valid_mask&1u)?q->default_note[0]:60,v=(int)kick_note(kick_root)-dn+64;
  while(v<40)v+=12;
  while(v>88)v-=12;
  p->sound[i].pitch=(uint8_t)v;
 }}
 if(q->generator==MC_KIT)drive_budget(p->extra);}
 else{
  /* Pattern plays 3..6 tracks: Density sets the centre (3 at 0%, 6 at 100%)
     and the seed moves it by one either way. Raising Density never drops a
     track for a given seed, since the track order is fixed by the seed. */
  {unsigned spread=draw(&s,3),centre=3+(d*3+50)/100+spread;active=centre<4?3:centre>7?6:centre-1;}
  if(q->generator==MC_PHRASE)active=1;
  /* Pattern replaces every track's locks; Phrase shares the pattern's 79 pairs. */
  budget=q->generator==MC_PATTERN?MC_LOCK_PAIRS:q->locks_elsewhere>=MC_LOCK_PAIRS?0:MC_LOCK_PAIRS-q->locks_elsewhere;
  if(q->generator==MC_PATTERN&&q->path==MC_RHYTHMIC)budget-=active;   /* pan_image: one (track, Pan) pair each */
  p->sequence_mask=q->generator==MC_PHRASE?(uint8_t)(1u<<q->target):0;
  if(q->generator==MC_PATTERN){p->sequence_mask=63;p->volume_mask=63;/* all six prior lines cleared, inactive ones stay empty */p->tempo=(uint16_t)(q->path==MC_ABSTRACT?range(&s,48,132):q->path==MC_MELODIC?range(&s,62,146):q->path==MC_RHYTHMIC?range(&s,76,172):range(&s,45,190));}
  for(i=0;i<active;i++){unsigned t=q->generator==MC_PHRASE?q->target:tracks[i];unsigned music_root=tonic;if(q->source_sound_valid_mask&(1u<<t))music_root=(tonic+88u-q->current_sound[t].pitch)%12;unsigned kick_floor=q->path==MC_RHYTHMIC&&roles[t]==MC_ANCHOR?60:0;
   if((q->machine_valid_mask&(1u<<t))&&q->current_machine[t]==0)kick_floor=86;
   if(q->source_sound_valid_mask&(1u<<t)){
    const mc_sound *a=&q->current_sound[t];
    if(a->machine==0||(a->machine==MC_MACRO&&a->shape==34)){
     /* Integer metadata rounds8.8 controls down, yielding a conservative
        lower bound. Fine offset is(fine-64)/32 semitones. */
     unsigned bound=((124u-a->pitch)*32u+64u-a->finetune+31u)/32u;
     kick_floor=bound<60?60:bound;
    }
   }
phrase(&p->phrase[t],&s,q->path,roles[t],d,b,length,root,draw(&s,8),family,kick_floor,music_root,music_scale);
   /* Rhythmic track 1 plays no melody: every hit sounds the kick root (the
      chosen root, or C) inside F1..E2 on the current kick sound. */
   if(q->path==MC_RHYTHMIC&&t==0){
    /* The kick sound's integer PITCH, even when other controls are fractional;
       64 if it is not known at all. */
    int base=(q->machine_valid_mask&1u)&&q->current_sound[0].pitch>=40&&q->current_sound[0].pitch<=88?q->current_sound[0].pitch:64,n=(int)kick_note(kick_root)-(base-64);
    while(n<0)n+=12;
    while(n>127)n-=12;
    for(j=0;j<64;j++)if(p->phrase[t].step[j].on)p->phrase[t].step[j].note=(uint8_t)n;
   }
   {art_ctx c;c.path=q->path;c.role=roles[t];c.density=d;c.brutality=b;c.kick=kick_floor!=0;c.music_root=music_root;c.music_scale=music_scale;
    c.machine=(q->machine_valid_mask&(1u<<t))?q->current_machine[t]:255;
    c.default_note=(q->note_valid_mask&(1u<<t))?q->default_note[t]:60;
    c.base_pitch=(q->source_sound_valid_mask&(1u<<t))?q->current_sound[t].pitch:64;
    c.pan_owned=q->generator==MC_PATTERN&&q->path==MC_RHYTHMIC;
    c.sound=(q->source_sound_valid_mask&(1u<<t))?&q->current_sound[t]:0;
    articulate(&p->phrase[t],&s,&c,&budget);}}
  if(q->generator==MC_PATTERN&&q->path==MC_RHYTHMIC)pan_image(p,tracks,active,q->seed,b);
 }
 return 1;
}
int mc_encode_engine(const mc_sound *s,uint8_t *buffer,unsigned size){unsigned i;uint8_t values[10];if(!s||!buffer||size<38||s->machine>=MC_MACHINES||s->pitch<40||s->pitch>88||s->punch>1||s->gate>1||(s->machine==MC_MACRO&&s->shape>46))return 0;values[0]=s->machine;values[1]=s->pitch;values[2]=s->color;values[3]=s->shape;values[4]=s->sweep;values[5]=s->contour;values[6]=s->punch;values[7]=s->gate;values[8]=s->finetune;values[9]=s->decay;for(i=0;i<10;i++)if(values[i]>127)return 0;for(i=0;i<10;i++){buffer[(9+i)*2]=values[i];buffer[(9+i)*2+1]=0;}return 1;}
int mc_encode_extras(const mc_sound_ext *e,uint8_t *words,unsigned size){
 static const uint8_t slots[12]={1,2,3,4,5,6,7,8,19,20,21,22},limits[12]={127,23,127,22,6,127,4,127,127,127,127,127};
 uint8_t values[12];unsigned i;
 if(!e||!words||size<64)return 0;
 for(i=0;i<6;i++)if(e->drive[i]>127)return 0;
 /* Byte 6: DJ filter position, int8 -63..63; byte 7: resonance 0..127. */
 if((int8_t)e->drive[6]<-63||(int8_t)e->drive[6]>63||e->drive[7]>127)return 0;
 values[0]=e->lfo_speed;values[1]=e->lfo_mult;values[2]=e->lfo_fade;values[3]=e->lfo_dest;values[4]=e->lfo_wave;values[5]=e->lfo_phase;
 values[6]=e->lfo_mode;values[7]=e->lfo_depth;values[8]=e->volume;values[9]=e->delay;values[10]=e->reverb;values[11]=e->pan;
 for(i=0;i<12;i++)if(values[i]>limits[i])return 0;
 /* Destinations that persist through a save: none, Pitch..Contour, Fine..Pan. */
 if(values[3]&&(values[3]<10||values[3]==15||values[3]==16))return 0;
 for(i=0;i<12;i++){words[2*slots[i]]=values[i];words[2*slots[i]+1]=0;}
 for(i=0;i<8;i++)words[56+i]=e->drive[i];
 return 1;
}
