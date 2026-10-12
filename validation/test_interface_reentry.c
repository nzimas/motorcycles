#include "../interface/interface.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls;
/* Notifications may dispatch input while publication is in progress. */
static int exit_during_publish(void *ctx,const mc_ui_request *r){mc_interface *s=ctx;assert(r->target==2&&r->path==1&&r->density==mc_level_amount(1));calls++;assert(s->state==MC_GENERATING);mc_event_handle(s,MC_PAGE,0,0,2,1600,0,0);return 1;}
static int release_during_publish(void *ctx,const mc_ui_request *r){mc_interface *s=ctx;(void)r;calls++;assert(mc_event_handle(s,MC_STEP,15,0,2,1600,0,0)==1);return 1;}
static void enter(mc_interface *s){mc_init(s);mc_event_handle(s,MC_PAGE,0,1,2,0,0,0);mc_event_handle(s,MC_PAD,0,1,2,1,0,0);mc_event_handle(s,MC_PAD,0,0,2,2,0,0);mc_event_handle(s,MC_PAGE,0,0,2,3,0,0);
 mc_event_handle(s,MC_STEP,0,1,2,4,0,0);mc_event_handle(s,MC_STEP,0,0,2,5,0,0);assert(s->state==MC_READY&&s->level[0]==1);}
int main(void){mc_interface s;
 /* PAGE tap exiting the latched mode while publication runs. */
 enter(&s);mc_event_handle(&s,MC_STEP,15,1,2,100,0,0);mc_event_handle(&s,MC_PAGE,0,1,2,1000,0,0);assert(s.page_exit);
 mc_event_handle(&s,MC_TICK,0,0,2,1599,exit_during_publish,&s);assert(!calls);
 assert(mc_event_handle(&s,MC_TICK,0,0,2,1600,exit_during_publish,&s)==0);
 assert(calls==1&&!s.page&&s.state==MC_INACTIVE&&!s.acknowledgement&&!s.generated&&mc_leds(&s,1600)==0&&mc_leds(&s,1700)==0);
 /* Step 16 release dispatched while publishing: generation still acknowledged, no retrigger. */
 calls=0;enter(&s);mc_event_handle(&s,MC_STEP,15,1,2,100,0,0);mc_event_handle(&s,MC_TICK,0,0,2,1600,release_during_publish,&s);
 assert(calls==1&&s.state==MC_READY&&!s.held&&!s.down&&s.acknowledgement==1&&s.generated&&!s.audition);
 mc_event_handle(&s,MC_TICK,0,0,2,5000,release_during_publish,&s);assert(calls==1);
 puts("PASS re-entrant PAGE release during publication retains inactive input/LED ownership; re-entrant step16 release keeps acknowledgement without retrigger");}
