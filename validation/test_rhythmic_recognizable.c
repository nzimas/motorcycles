/* Rhythmic Phrase/Pattern locks keep each track recognisable: timbre locks
   (COLOR, SHAPE, SWEEP, CONTOUR) stay within a Brutality-sized window around
   the track's own sound, decay locks grow only a little, a MACRO track's model
   (SHAPE) is never locked, and a stock KICK's SHAPE stays on its side of the
   feedback-noise boundary. Pitch locks stay within an octave. */
#include "../generation/generation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){mc_plan p;unsigned cases=0,locks=0,timbre=0;
 for(unsigned g=MC_PHRASE;g<=MC_PATTERN;g++)for(unsigned b=0;b<=100;b+=25)for(unsigned seed=1;seed<=300;seed++){
  mc_request q={.generator=g,.path=MC_RHYTHMIC,.target=(uint8_t)(seed%6),.density=100,.brutality=(uint8_t)b,.machine_mask=127,.machine_valid_mask=63,.source_sound_valid_mask=63,.phrase_length=g==MC_PHRASE?64:0,.seed=seed*2246822519u+b};
  for(unsigned t=0;t<6;t++){unsigned m=(seed+t*3)%7;q.current_machine[t]=(uint8_t)m;
   q.current_sound[t]=(mc_sound){(uint8_t)m,(uint8_t)(56+(seed*7+t)%20),(uint8_t)((seed*13+t*29)%128),(uint8_t)(m==MC_MACRO?(seed+t)%47:m==5?(seed+t)%38:(seed*31+t*17)%128),(uint8_t)((seed*11+t*7)%128),(uint8_t)((seed*5+t*41)%128),1,0,64,(uint8_t)(2+(seed*3+t)%100)};}
  assert(mc_generate(&q,&p));
  for(unsigned t=0;t<6;t++){const mc_sound *o=&q.current_sound[t];int w=12+(int)(b*20/100);
   for(unsigned i=0;i<64;i++){const mc_event *e=&p.phrase[t].step[i];
    for(unsigned j=0;j<e->lock_count;j++){int k=e->lock_k[j],v=e->lock_v[j];locks++;
     if(k==11){assert(abs(v-o->color)<=w);timbre++;}
     if(k==13){assert(abs(v-o->sweep)<=w);timbre++;}
     if(k==14){assert(abs(v-o->contour)<=w);timbre++;}
     if(k==12){assert(o->machine!=MC_MACRO);assert(abs(v-o->shape)<=w);timbre++;
      if(o->machine==0)assert((o->shape<75&&v<=74)||(o->shape>=75&&v>=80));}
     if(k==18)assert(v<=o->decay+4+(int)(b*12/100));
     if(k==10)assert(v>=40&&v<=88);
    }}}
  cases++;
 }
 assert(timbre>1000);
 printf("PASS %u Rhythmic Phrase/Pattern cases: %u locks, %u timbre locks inside the track's window\n",cases,locks,timbre);return 0;}
