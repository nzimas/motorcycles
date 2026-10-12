#include "../generation/generation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int kick(const mc_sound *s){return s->machine==0||(s->machine==MC_MACRO&&s->shape==34);}
int main(void){mc_request q={.generator=MC_PATCH,.target=0}; /* track 1: the Rhythmic kick */mc_plan low,high,dense;unsigned checks=0;
 for(unsigned machine=0;machine<=MC_MACRO;machine+=MC_MACRO)for(unsigned path=0;path<4;path++){
  unsigned matched=0;q.path=path;q.machine_mask=1u<<machine;
  for(unsigned seed=1;seed<=4096;seed++){
   q.seed=seed;q.density=0;q.brutality=0;assert(mc_generate(&q,&low));q.density=100;assert(mc_generate(&q,&dense));assert(!memcmp(&low,&dense,sizeof low));q.brutality=100;assert(mc_generate(&q,&high));
   const mc_sound *a=&low.sound[0],*b=&high.sound[0];
   if(path==MC_MELODIC)assert(!memcmp(&low,&high,sizeof low));
   else if(kick(a)&&kick(b)){
    /* Same kick; on Rhythmic track 1 a higher level only lengthens the decay within its character. */
    assert(a->machine==b->machine&&a->pitch==b->pitch&&a->finetune==b->finetune);
    if(path==MC_RHYTHMIC)assert(b->decay>=a->decay);else assert(a->decay==b->decay);
    /* Brutality pushes timbre and/or Volume+Dist, never backing off the drive. */
    assert(high.extra[0].volume>=low.extra[0].volume);
    if(path==MC_RHYTHMIC){assert(high.extra[0].drive[0]>=low.extra[0].drive[0]&&high.extra[0].drive[2]>=low.extra[0].drive[2]);
     assert(a->color!=b->color||a->shape!=b->shape||a->sweep!=b->sweep||high.extra[0].drive[0]>low.extra[0].drive[0]);}
    else assert(a->color!=b->color||a->shape!=b->shape||a->sweep!=b->sweep||a->contour!=b->contour||high.extra[0].volume>low.extra[0].volume);matched++;
   }
   checks++;
  }
  if(path!=MC_MELODIC)assert(matched);
 }
 printf("PASS %u kick same-seed Density inactivity, Brutality timbre and Melodic inactivity cases\n",checks);return 0;}
