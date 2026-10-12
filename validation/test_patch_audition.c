#include "../interface/interface.h"
#include <assert.h>
#include <stdio.h>
static unsigned pubs;static int success=1;
static int publish(void *ctx,const mc_ui_request *q){(void)ctx;assert(q->target==4&&q->path==1);pubs++;return success;}
static void event(mc_interface *s,unsigned e,unsigned i,int v,unsigned now){mc_event_handle(s,e,i,v,0,now,publish,0);}
static void tap(mc_interface *s,unsigned at,unsigned ms){event(s,MC_STEP,15,1,at);event(s,MC_STEP,15,0,at+ms);}
int main(void){mc_interface s;mc_init(&s);mc_event_handle(&s,MC_PAGE,0,1,4,0,publish,0);mc_event_handle(&s,MC_PAD,0,1,4,1,publish,0);event(&s,MC_PAD,0,0,2);event(&s,MC_PAGE,0,0,3);assert(s.state==MC_READY&&!s.page);
 tap(&s,10,20);assert(!pubs&&!s.audition);                                   /* nothing generated yet */
 event(&s,MC_STEP,15,1,100);event(&s,MC_TICK,0,0,1599);assert(!pubs);event(&s,MC_TICK,0,0,1600);assert(pubs==1&&s.generated&&!s.audition);event(&s,MC_STEP,15,0,1601);assert(!s.audition);
 tap(&s,1700,599);assert(s.audition&&pubs==1);s.audition=0;                  /* short tap after success auditions */
 tap(&s,2400,MC_AUDITION_TAP_MS);assert(!s.audition&&pubs==1);               /* 600 ms is no longer a tap */
 tap(&s,3100,900);assert(!s.audition&&pubs==1);                              /* long release without tick never auditions */
 tap(&s,4000,1);assert(s.audition);s.audition=0;tap(&s,4100,100);assert(s.audition);s.audition=0; /* repeatable */
 event(&s,MC_STEP,15,1,4300);event(&s,MC_TICK,0,0,5800);event(&s,MC_STEP,15,0,5801);assert(pubs==2&&!s.audition);
 event(&s,MC_STEP,4,1,5900);event(&s,MC_STEP,4,0,5901);assert(s.level[0]==5);  /* level keys never audition */
 assert(!s.audition);
 event(&s,MC_PAD,1,1,6000);event(&s,MC_PAD,1,0,6001);assert(!s.generated&&!s.audition&&s.generator==1);tap(&s,6100,50);assert(!s.audition);
 event(&s,MC_STEP,15,1,6200);event(&s,MC_TICK,0,0,7700);event(&s,MC_STEP,15,0,7701);assert(pubs==3&&!s.generated);tap(&s,7800,50);assert(!s.audition); /* Kit never auditions */
 event(&s,MC_PAD,0,1,7900);event(&s,MC_PAD,0,0,7901);assert(!s.generated);tap(&s,8000,50);assert(!s.audition); /* switching back invalidates */
 success=0;event(&s,MC_STEP,15,1,8100);event(&s,MC_TICK,0,0,9600);event(&s,MC_STEP,15,0,9601);assert(pubs==4&&!s.generated&&s.acknowledgement==2);tap(&s,9700,100);assert(!s.audition); /* failure never auditions */
 success=1;event(&s,MC_STEP,15,1,9800);event(&s,MC_TICK,0,0,11300);event(&s,MC_STEP,15,0,11301);assert(pubs==5&&s.generated);
 event(&s,MC_STEP,15,1,11400);event(&s,MC_PAGE,0,1,11450);event(&s,MC_PAGE,0,0,11451);assert(s.state==MC_INACTIVE&&!s.audition&&!s.generated);
 event(&s,MC_STEP,15,0,11460);assert(!s.audition);
 puts("PASS latched PAGE, successful-Patch-only step16 audition tap (<600 ms), 1500 ms hold threshold, generator/failure invalidation and PAGE exit");return 0;}
