/* Rhythmic Pattern stereo image: every playing track carries a pan lock on
   every hit; the kick is centred on every quarter (so every downbeat) and
   stays near the centre otherwise; the other voices are spread across both
   sides in a balanced way and move within their track; Phrase kicks with a
   random pan lane keep their quarters centred too. */
#include "../generation/generation.h"
#include "../generation/music.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int pan_of(const mc_event *e){int v=-1,n=0;for(unsigned j=0;j<e->lock_count;j++)if(e->lock_k[j]==22){v=e->lock_v[j];n++;}assert(n<=1);return v;}
int main(void){mc_plan p;unsigned cases=0,moving=0,kick_alt=0,tracks_seen=0,both_sides=0;long balance_sum=0;
 for(unsigned b=0;b<=100;b+=20)for(unsigned d=0;d<=100;d+=25)for(unsigned seed=1;seed<=60;seed++){
  mc_request q={.generator=MC_PATTERN,.path=MC_RHYTHMIC,.density=d,.brutality=b,.machine_mask=127,.seed=seed*2654435761u+b+d};
  assert(mc_generate(&q,&p));
  int left=0,right=0;long mean_sum=0;unsigned voices=0;
  for(unsigned t=0;t<6;t++){const mc_phrase *ph=&p.phrase[t];unsigned hits=0,lo=127,hi=0;long sum=0;
   for(unsigned i=0;i<ph->length;i++){const mc_event *e=&ph->step[i];if(!e->on)continue;
    int v=pan_of(e);assert(v>=0&&v<=127);hits++;sum+=v;if((unsigned)v<lo)lo=(unsigned)v;if((unsigned)v>hi)hi=(unsigned)v;
    if(t==0){if(i%4==0)assert(v==64);else{assert(abs(v-64)<=40);}}
   }
   if(!hits)continue;
   if(t==0){assert(lo<=64&&hi>=64);kick_alt+=hi>64;continue;}
   tracks_seen++;voices++;moving+=hi>lo;
   long mean=sum/(long)hits;mean_sum+=mean-64;if(lo<56)left++;if(hi>72)right++;
  }
  /* Two or more voices besides the kick: both sides used whenever 3+ voices. */
  if(voices>=3){assert(left&&right);both_sides++;}
  balance_sum+=labs(mean_sum)/(voices?voices:1);
  cases++;
 }
 assert(moving*10>tracks_seen*6);           /* most voices move within the track */
 assert(kick_alt*5>cases);                  /* some kicks alternate off the quarters */
 assert(balance_sum/(long)cases<=24);       /* average voice offset stays balanced */
 /* Phrase: a random pan lane on a Rhythmic kick keeps quarters centred. */
 for(unsigned seed=1;seed<=4000;seed++){
  mc_request q={.generator=MC_PHRASE,.path=MC_RHYTHMIC,.target=0,.density=100,.brutality=100,.machine_mask=127,.phrase_length=32,.seed=seed*7919u};
  assert(mc_generate(&q,&p));
  for(unsigned i=0;i<64;i++){const mc_event *e=&p.phrase[0].step[i];int v=pan_of(e);if(v<0)continue;
   if(i%4==0)assert(v==64);else assert(v>=44&&v<=84);}
 }
 printf("PASS %u Rhythmic patterns: %u voices (%u moving), %u alternating kicks, %u multi-voice images on both sides\n",cases,tracks_seen,moving,kick_alt,both_sides);return 0;}
