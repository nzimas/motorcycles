/* Unified rhythmic path: LYRA is only ever a texture (track 5 in a Kit or
   Patch; long decay or GATE, no PUNCH), texture tracks may borrow Abstract
   recipes, and texture lines in Phrase/Pattern (including any LYRA track) get
   few, long notes. */
#include "../generation/generation.h"
#include <assert.h>
#include <stdio.h>
int main(void){mc_plan p;unsigned lyra=0,textures=0,cases=0,long_notes=0,notes=0;
 for(unsigned g=MC_PATCH;g<=MC_KIT;g++)for(unsigned b=0;b<=100;b+=25)for(unsigned seed=1;seed<=800;seed++){
  mc_request q={.generator=g,.path=MC_RHYTHMIC,.target=(uint8_t)(seed%6),.brutality=(uint8_t)b,.machine_mask=127,.seed=seed*2654435761u};
  assert(mc_generate(&q,&p));
  for(unsigned t=0;t<6;t++){if(!(p.sound_mask&(1u<<t)))continue;const mc_sound*o=&p.sound[t];
   if(o->machine==5){lyra++;assert(t==4);assert(o->decay>=60||o->gate);assert(!o->punch);}
   if(t==4)textures++;}
  cases++;}
 assert(lyra>50);
 for(unsigned d=0;d<=100;d+=50)for(unsigned seed=1;seed<=300;seed++){
  mc_request q={.generator=MC_PATTERN,.path=MC_RHYTHMIC,.density=(uint8_t)d,.brutality=50,.machine_mask=127,.machine_valid_mask=63,.seed=seed*40503u};
  for(unsigned t=0;t<6;t++)q.current_machine[t]=(uint8_t)(t==0?0:t==3?5:t%5);
  assert(mc_generate(&q,&p));
  const mc_phrase *ph=&p.phrase[3];unsigned n=0;
  for(unsigned i=0;i<64;i++)if(ph->step[i].on){n++;notes++;long_notes+=ph->step[i].length>=4;assert(ph->step[i].length>=4&&ph->step[i].length<=12);}
  assert(!ph->length||n<=1u+ph->length/4u);
  cases++;}
 assert(notes>100);
 printf("PASS %u unified-rhythmic cases: %u LYRA textures (track 5 only), %u texture tracks, %u long texture notes\n",cases,lyra,textures,long_notes);return 0;}
