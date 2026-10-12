#include "../interface/interface.h"
#include <stddef.h>
#define COUNT 64
#define WIDTH 43
uint8_t output[COUNT*WIDTH];
struct guarded {uint8_t before[16];mc_interface ui;uint8_t after[16];} state;
/* Read by the proof to locate the suffix guard independent of target struct layout. */
const uint32_t guard_after_offset=offsetof(struct guarded,after),snapshot_width=WIDTH,snapshot_count=COUNT;
uint32_t publications;mc_ui_request request;
static int publish(void *ctx,const mc_ui_request*r){(void)ctx;request=*r;publications++;return publications%3!=0;}
struct event {unsigned e,i;int v;unsigned target;uint32_t now;};
static const struct event events[COUNT]={
#include "interface-target-events.inc"
};
static void w32(uint8_t *p,uint32_t x){p[0]=x>>24;p[1]=x>>16;p[2]=x>>8;p[3]=x;}
void run_suite(void){unsigned k,g,n;mc_interface *s=&state.ui;mc_init(s);publications=0;
 for(k=0;k<16;k++){state.before[k]=0x5a;state.after[k]=0xa5;}
 for(k=0;k<COUNT;k++){const struct event *e=&events[k];uint8_t *o=&output[k*WIDTH];
 int consumed=mc_event_handle(s,e->e,e->i,e->v,e->target,e->now,publish,0);
 n=0;o[n++]=consumed;o[n++]=s->state;o[n++]=s->page;o[n++]=s->generator;o[n++]=s->path;o[n++]=s->target;o[n++]=s->held;o[n++]=s->down;o[n++]=s->pad_down;o[n++]=s->armed;o[n++]=s->fired;o[n++]=s->feedback;o[n++]=s->acknowledgement;
 for(g=0;g<4;g++)o[n++]=s->level[g];
 o[n++]=(uint8_t)(s->level_down>>8);o[n++]=(uint8_t)s->level_down;o[n++]=s->generated;o[n++]=s->audition;o[n++]=s->page_exit;
 w32(o+n,s->since);n+=4;w32(o+n,s->feedback_until);n+=4;w32(o+n,s->acknowledgement_until);n+=4;
 o[n++]=request.generator;o[n++]=request.path;o[n++]=request.target;o[n++]=request.density;o[n++]=request.brutality;o[n++]=request.music;

 o[n++]=(uint8_t)publications;o[n++]=(uint8_t)(mc_leds(s,e->now)&255);o[n++]=(uint8_t)(mc_leds(s,e->now)>>8);
 }
}
#ifdef HOST
#include <stdio.h>
int main(void){run_suite();return fwrite(output,1,sizeof output,stdout)!=sizeof output;}
#endif
