#include "../interface/interface.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls;static int result=1;static mc_ui_request request;
static int publish(void *ctx,const mc_ui_request *r){(void)ctx;assert(r->generator==4);request=*r;calls++;return result;}
static int ev(mc_interface *s,unsigned e,unsigned i,int v,unsigned now){return mc_event_handle(s,e,i,v,5,now,publish,0);}
int main(void){mc_interface s;mc_init(&s);assert(s.music==0x99);ev(&s,MC_PAGE,0,1,0);assert(ev(&s,MC_PAD,4,1,1)&&s.state==MC_SCALE);ev(&s,MC_PAD,4,0,2);ev(&s,MC_PAGE,0,0,3);assert(s.state==MC_SCALE);
 for(unsigned i=0;i<16;i++){unsigned prior=calls;assert(ev(&s,MC_STEP,i,1,10+i*10));assert(calls==prior+1&&s.state==MC_SCALE&&s.music==request.music);assert(ev(&s,MC_STEP,i,1,11+i*10)&&calls==prior+1);assert(ev(&s,MC_STEP,i,0,12+i*10));if(i<4)assert(((s.music>>4)&3)==i);else assert((s.music&15)==i-4);assert(mc_leds(&s,0)==0xffff);assert(mc_leds(&s,400)==(uint16_t)(0xffffu&~((1u<<((s.music>>4)&3))|(1u<<(4+(s.music&15))))));}
 unsigned prior=calls;uint8_t music=s.music;assert(ev(&s,MC_DENSITY,0,40,200)==1&&ev(&s,MC_BRUTALITY,0,40,201)==1);assert(calls==prior&&s.music==music);
 result=0;ev(&s,MC_STEP,0,1,220);ev(&s,MC_STEP,0,0,221);assert(s.music==music&&s.acknowledgement==2);
 ev(&s,MC_PAGE,0,1,230);ev(&s,MC_PAGE,0,0,231);assert(s.state==MC_INACTIVE&&mc_leds(&s,232)==0);assert(ev(&s,MC_STEP,15,0,233)==0);
 /* Generator mode <-> scale menu: levels kept, knobs consumed only in the menu. */
 mc_init(&s);ev(&s,MC_PAGE,0,1,300);ev(&s,MC_PAD,1,1,301);ev(&s,MC_PAD,1,0,302);ev(&s,MC_STEP,10,1,303);ev(&s,MC_STEP,10,0,304);assert(s.state==MC_READY&&s.level[1]==11&&s.target==5);
 assert(ev(&s,MC_DENSITY,0,5,305)==0);assert(ev(&s,MC_PAD,4,1,306)&&s.state==MC_SCALE);ev(&s,MC_PAD,4,0,307);assert(ev(&s,MC_DENSITY,0,5,308)==1&&ev(&s,MC_BRUTALITY,0,5,309)==1);
 assert(mc_leds(&s,0)==0xffff);assert(ev(&s,MC_STEP,15,1,310)&&calls==prior+2&&s.state==MC_SCALE);ev(&s,MC_STEP,15,0,311);
 ev(&s,MC_PAD,1,1,312);ev(&s,MC_PAD,1,0,313);ev(&s,MC_PAGE,0,0,314);assert(s.state==MC_READY&&s.generator==1&&s.level[1]==11&&mc_leds(&s,2000)==0x7ff&&ev(&s,MC_BRUTALITY,0,5,315)==0);
 ev(&s,MC_PAGE,0,1,316);ev(&s,MC_PAGE,0,0,317);assert(s.state==MC_INACTIVE);
 puts("PASS all16 scale/root steps, duplicate/release ownership, latchedmenu, knobinactivity, refusalpreservesmusic, PAGEexit, generator<->menu level retention and knob ownership");return 0;}
