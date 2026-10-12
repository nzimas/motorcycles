#include "../generation/generation.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
/* MACRO models allowed per role: snare/struck/chaotic FM/noise sets, and cymbal/noises. */
#define NOISY ((UINT64_C(0x3f)<<41)|(UINT64_C(1)<<16)|(UINT64_C(1)<<13)|(UINT64_C(3)<<26)|(UINT64_C(1)<<38)|(UINT64_C(1)<<15))
#define IMPACT (NOISY|(UINT64_C(1)<<36)|(UINT64_C(1)<<33))
#define DETAIL ((UINT64_C(1)<<35)|(UINT64_C(1)<<43)|(UINT64_C(1)<<41)|(UINT64_C(1)<<45))
static unsigned hits(const mc_phrase *p){unsigned n=0;for(unsigned i=0;i<64;i++)n+=p->step[i].on;return n;}
int main(void){mc_request q={.generator=MC_KIT,.path=MC_RHYTHMIC,.target=5,.density=50,.brutality=50,.machine_mask=255,.machine_valid_mask=63};mc_plan p,old;unsigned cases=0;
 for(unsigned seed=1;seed<=2048;seed++){
  q.seed=seed;assert(mc_generate(&q,&p));assert(p.sound_mask==63&&!p.sequence_mask&&!p.tempo);
  assert(p.sound[0].machine==0||(p.sound[0].machine==MC_MACRO&&p.sound[0].shape==34));
  /* Track2 impact: snare, metal, tone noise or MACRO snare/struck/chaotic/noise. */
  assert(p.sound[1].machine==1||p.sound[1].machine==2||p.sound[1].machine==4||(p.sound[1].machine==MC_MACRO&&((IMPACT>>p.sound[1].shape)&1)));
  /* Track3 detail: metal, perc, tone or MACRO cymbal/noises. */
  assert(p.sound[2].machine==2||p.sound[2].machine==3||p.sound[2].machine==4||(p.sound[2].machine==MC_MACRO&&((DETAIL>>p.sound[2].shape)&1)));
  /* LYRA (machine 5, in place of CHORD) may play percussion on tracks 4..6. */
  for(unsigned t=1;t<6;t++)assert(p.sound[t].machine>=1&&p.sound[t].machine<=4||(t>=3&&p.sound[t].machine==5)||(p.sound[t].machine==MC_MACRO&&p.sound[t].shape!=34));
  cases++;
 }
 memset(&p,0xa5,sizeof p);old=p;
 for(unsigned mask=32;mask<=128;mask*=4)  /* LYRA only (no kick), or no real machine */{q.machine_mask=mask;assert(!mc_generate(&q,&p));assert(!memcmp(&p,&old,sizeof p));cases++;}
 q.generator=MC_PATTERN;q.machine_mask=255;memset(q.current_machine,5,6);
 for(unsigned density=0;density<=100;density++){
  q.density=density;q.seed=113+density;assert(mc_generate(&q,&p));assert(!p.sound_mask&&p.sequence_mask==63);
  static const unsigned layout[6]={MC_ANCHOR,MC_IMPACT,MC_DETAIL,MC_RESPONSE,MC_TEXTURE,MC_IMPACT};
  /* Every track here is LYRA: tracks 2..6 sequence as textures; track 1 stays the kick. */
  unsigned active=0;for(unsigned t=0;t<6;t++)if(hits(&p.phrase[t])){active++;assert(p.phrase[t].role==(t?MC_TEXTURE:layout[t]));}
  assert(active>=3&&active<=6&&hits(&p.phrase[0]));cases++;
 }
 /* Any combination of tracks 2..6 joins the kick; more Density never drops a
    track for a given seed; the track count varies between seeds. */
 {unsigned seen[64]={0},on_count[6]={0},sizes=0;
  for(unsigned seed=1;seed<=512;seed++){unsigned previous=0;q.seed=seed;
   for(unsigned density=0;density<=100;density+=10){q.density=density;assert(mc_generate(&q,&p));unsigned mask=0;
    for(unsigned t=0;t<6;t++)if(hits(&p.phrase[t]))mask|=1u<<t;
    assert((mask&1)&&(mask&previous)==previous);previous=mask;
    if(density==40){seen[mask]=1;sizes|=1u<<__builtin_popcount(mask);for(unsigned t=0;t<6;t++)on_count[t]+=(mask>>t)&1;}cases++;}}
  unsigned distinct=0;for(unsigned m=0;m<64;m++)distinct+=seen[m];
  assert(distinct>=20&&(sizes&(1u<<3))&&(sizes&(1u<<4))&&(sizes&(1u<<5)));
  for(unsigned t=1;t<6;t++)assert(on_count[t]>50&&on_count[t]<500);}
 printf("PASS %u independent rhythmic role and atomic failure cases\n",cases);return 0;}
