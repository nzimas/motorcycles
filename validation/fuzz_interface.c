#include "../interface/interface.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
static uint32_t rng=0x193ca125;static unsigned calls;
static uint32_t next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static int publish(void *c,const mc_ui_request*r){mc_interface *s=c;assert(s->state==MC_GENERATING);
 if(r->generator<4){assert(s->held&&s->fired&&r->generator==s->generator&&r->path==1&&r->music==0);
  assert(r->density==mc_level_amount(s->level[s->generator])&&r->brutality==r->density);}
 else assert(r->generator==4&&r->path==0&&(r->music&128)&&!r->density&&!r->brutality);
 assert(r->target<6&&r->density<=100&&r->brutality<=100);calls++;return calls%3!=0;}
int main(void){mc_interface s,before;uint32_t now=UINT32_MAX-500;unsigned k,g,levels=0,auditions=0,pairs=0,violations=0;uint32_t phys=0,owned=0;mc_init(&s);
 for(k=0;k<1000000;k++){
  /* Weighted: steps (half of them step 16) and ticks dominate, PAGE is rarer. */
  static const uint8_t mix[16]={MC_PAGE,MC_PAD,MC_PAD,MC_STEP,MC_STEP,MC_STEP,MC_STEP,MC_STEP,MC_STEP,MC_DENSITY,MC_BRUTALITY,MC_TICK,MC_TICK,MC_TICK,MC_TICK,MC_TICK};
  unsigned e=mix[next()%16],index,target=next()%8,oldcalls=calls;int value;
  index=e==MC_STEP?(next()%2?15:next()%18):next()%8;
  if(e==MC_DENSITY||e==MC_BRUTALITY)value=(int)(next()%401)-200;else value=next()%2;
  now+=next()%4==0?next()%1700:next()%250;before=s;
  int consumed=mc_event_handle(&s,e,index,value,target,now,publish,&s);
  assert(consumed==0||consumed==1);
  /* Physical press/release pairing: an owned (consumed) press must have its release
     consumed, even after PAGE exit. (Releases of keys pressed before entering a mode
     are consumed by design while the mode is active, so only this direction holds.) */
  if((e==MC_STEP&&index<16)||(e==MC_PAD&&index<6)){uint32_t bit=1u<<(e==MC_PAD?16+index:index);
   if(value&&!(phys&bit)){phys|=bit;if(consumed)owned|=bit;else owned&=~bit;}
   else if(!value&&(phys&bit)){phys&=~bit;pairs++;
    if((owned&bit)&&!consumed&&!violations++){fprintf(stderr,"first pairing violation at event %u: %s %u press %s, release %s (state %u->%u)\n",k,e==MC_PAD?"pad":"step",index+1,owned&bit?"consumed":"ordinary",consumed?"consumed":"ordinary",before.state,s.state);}}}
  assert(s.state<=MC_SCALE&&s.state!=MC_GENERATOR&&s.generator<4&&s.target<6);assert(!(s.down&~1u)&&!(s.pad_down&~63)&&!(s.level_down&~0x7fffu));
  if(s.state!=MC_INACTIVE&&s.state!=MC_SCALE)assert(s.path==1);
  for(g=0;g<4;g++)assert(s.level[g]>=1&&s.level[g]<=15);
  if(calls!=oldcalls){assert(calls==oldcalls+1);assert((e==MC_TICK&&before.state==MC_HOLD&&before.held&&!before.fired&&(uint32_t)(now-before.since)>=MC_GENERATE_HOLD_MS)||(e==MC_STEP&&value&&before.state==MC_SCALE));}
  if(e==MC_TICK&&before.state==MC_HOLD&&before.held&&!before.fired&&(uint32_t)(now-before.since)>=MC_GENERATE_HOLD_MS)assert(calls==oldcalls+1&&s.state==MC_READY&&s.acknowledgement==(calls%3?1:2));
  if(e==MC_TICK)assert(!consumed);
  if((e==MC_DENSITY||e==MC_BRUTALITY))assert(consumed==(before.state==MC_SCALE));
  for(g=0;g<4;g++)if(s.level[g]!=before.level[g]){assert(e==MC_STEP&&value&&index<15&&g==before.generator&&s.level[g]==index+1&&(before.state==MC_READY||before.state==MC_HOLD)&&consumed);levels++;}
  if(e==MC_STEP&&value&&index<15&&(before.state==MC_READY||before.state==MC_HOLD)&&!(before.level_down&(1u<<index)))assert(s.level[s.generator]==index+1&&(s.level_down&(1u<<index)));
  if(!before.audition&&s.audition){assert(e==MC_STEP&&!value&&index==15&&before.generator==0&&before.generated&&before.state==MC_HOLD&&!before.fired&&(uint32_t)(now-before.since)<MC_AUDITION_TAP_MS);auditions++;}
  if(e==MC_PAGE&&!value&&before.page_exit){assert(s.state==MC_INACTIVE&&!s.page&&!s.held&&!s.armed&&!s.fired&&!s.audition&&mc_leds(&s,now)==0);}
  if(before.state!=MC_INACTIVE&&!(e==MC_PAGE&&!value))assert(s.target==before.target);
  {uint16_t leds=mc_leds(&s,now);if(s.state==MC_INACTIVE)assert(!leds);
   else if(s.state!=MC_SCALE&&!(s.acknowledgement&&(int32_t)(now-s.acknowledgement_until)<0))assert((leds&0x7fff)==(uint16_t)((1u<<s.level[s.generator])-1u));}
 }
 assert(levels&&auditions);
 if(violations){printf("FAIL %u owned press/release pairing violations (release of a consumed press dispatched to ordinary handling)\n",violations);return 1;}
 printf("PASS 1000000 randomized ordered events; %u guarded publications; %u level changes; %u auditions; %u press/release pairs; sanitizer clean\n",calls,levels,auditions,pairs);return 0;}
