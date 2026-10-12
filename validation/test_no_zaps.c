/* No laser/pew zaps or sirens: SNARE and PERC pitch sweeps stay small, and
   pitch or fine tune is only ever modulated at audio rate (FM grit), never by
   an envelope, a slow drift or per-hit random steps. */
#include "../generation/generation.h"
#include <assert.h>
#include <stdio.h>
int main(void){mc_plan p;unsigned sounds=0,lfos=0;
 for(unsigned g=MC_PATCH;g<=MC_KIT;g++)for(unsigned path=0;path<4;path++)for(unsigned b=0;b<=100;b+=50)for(unsigned seed=1;seed<=3000;seed++){
  mc_request q={.generator=g,.path=path,.target=seed%6,.brutality=b,.machine_mask=255,.seed=seed*2654435761u};
  assert(mc_generate(&q,&p));
  for(unsigned t=0;t<6;t++){if(!(p.sound_mask&(1u<<t)))continue;const mc_sound *s=&p.sound[t];const mc_sound_ext *e=&p.extra[t];sounds++;
   if(s->machine==1||s->machine==3)assert(s->sweep<=50);
   if(e->lfo_dest==10||e->lfo_dest==17){assert(e->lfo_speed>=112&&e->lfo_mult>=22);lfos++;}
  }
 }
 assert(lfos);printf("PASS %u sounds: no SNARE/PERC zaps, pitch modulated only at audio rate (%u cases)\n",sounds,lfos);return 0;}
