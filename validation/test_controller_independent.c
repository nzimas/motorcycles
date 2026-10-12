#include "../interface/controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;static mc_plan last;static mc_request request;
static int commit(void *ctx,const mc_plan *p,const mc_request*r){assert(ctx==(void *)42);last=*p;request=*r;calls++;return 1;}
static void ev(mc_controller*c,unsigned e,unsigned i,int v,unsigned target,uint32_t now){mc_controller_event(c,e,i,v,target,now);}
/* PAGE + pad g on track 4, then step `level` sets the generator's level. */
static void select(mc_controller*c,unsigned g,unsigned level){ev(c,MC_PAGE,0,1,4,0);ev(c,MC_PAD,g,1,4,1);ev(c,MC_PAD,g,0,0,2);ev(c,MC_STEP,level-1,1,0,3);ev(c,MC_STEP,level-1,0,0,4);}
/* Step 16 held for exactly MC_GENERATE_HOLD_MS generates once. */
static void hold(mc_controller*c,uint32_t now){ev(c,MC_STEP,15,1,0,now);ev(c,MC_TICK,0,0,0,now+MC_GENERATE_HOLD_MS-1);ev(c,MC_TICK,0,0,0,now+MC_GENERATE_HOLD_MS);ev(c,MC_STEP,15,0,0,now+MC_GENERATE_HOLD_MS+1);}
int main(void){mc_controller c;mc_plan prior;uint32_t seed;unsigned n;
 mc_controller_init(&c,0x61c88647u,63,commit,(void *)42);select(&c,MC_PHRASE,9);
 c.machine_valid_mask=1u<<4;c.current_machine[4]=0;hold(&c,10);assert(calls==1&&request.target==4&&request.generator==MC_PHRASE&&request.path==MC_RHYTHMIC&&request.density==mc_level_amount(9)&&request.brutality==request.density&&request.density==57&&c.seed==0x6d2b79f5u&&request.seed!=0&&request.machine_valid_mask==16&&last.phrase[4].role==MC_ANCHOR&&last.sequence_mask==16);
 prior=last;seed=c.seed;n=calls;ev(&c,MC_STEP,15,1,0,1000);ev(&c,MC_PAGE,0,0,0,1198);ev(&c,MC_PAGE,0,1,0,1199);ev(&c,MC_PAGE,0,0,0,1200);ev(&c,MC_TICK,0,0,0,3000);assert(calls==n&&c.seed==seed&&!memcmp(&last,&prior,sizeof last));ev(&c,MC_STEP,15,0,0,3001);
 select(&c,MC_PATCH,MC_LEVEL_DEFAULT);c.machine_mask=0;seed=c.seed;n=calls;hold(&c,3010);assert(c.seed!=seed&&calls==n&&c.ui.acknowledgement==2&&!memcmp(&last,&prior,sizeof last));
 c.machine_mask=63;c.current_machine[4]=7;seed=c.seed;  /* not a machine on this build */hold(&c,5000);assert(c.seed!=seed&&calls==n&&c.ui.acknowledgement==2);
 c.current_machine[4]=0;c.commit=0;hold(&c,7000);assert(calls==n&&c.ui.acknowledgement==2);
 c.commit=commit;c.machine_valid_mask=0;hold(&c,9000);assert(calls==n+1&&c.ui.acknowledgement==1&&request.target==4&&request.generator==MC_PATCH&&request.density==mc_level_amount(MC_LEVEL_DEFAULT)&&last.sound_mask==16);
 ev(&c,MC_STEP,12,1,0,11000);ev(&c,MC_STEP,12,0,0,11001);assert(c.ui.level[MC_PATCH]==13&&c.ui.level[MC_PHRASE]==9);
 ev(&c,MC_PAGE,0,0,0,11002);ev(&c,MC_PAGE,0,1,0,11003);ev(&c,MC_PAGE,0,0,0,11004);assert(c.ui.state==MC_INACTIVE);
 ev(&c,MC_PAGE,0,1,4,12000);ev(&c,MC_PAD,MC_PATCH,1,4,12001);ev(&c,MC_PAD,MC_PATCH,0,4,12002);assert(c.ui.level[MC_PATCH]==13);
 n=calls;hold(&c,12010);assert(calls==n+1&&request.density==mc_level_amount(13)&&request.brutality==mc_level_amount(13));
 mc_controller_init(&c,123,63,commit,(void *)42);assert(c.ui.level[MC_PATCH]==MC_LEVEL_DEFAULT&&c.ui.level[MC_PHRASE]==MC_LEVEL_DEFAULT&&c.machine_valid_mask==0&&c.seed==123);
 /* Same events at the same times repeat the seed; other timings change it. */
 {mc_controller a,b2;uint32_t sa,sb;mc_controller_init(&a,7,63,commit,(void *)42);mc_controller_init(&b2,7,63,commit,(void *)42);
  select(&a,MC_PATCH,MC_LEVEL_DEFAULT);hold(&a,100);sa=request.seed;select(&b2,MC_PATCH,MC_LEVEL_DEFAULT);hold(&b2,100);assert(request.seed==sa);
  mc_controller_init(&b2,7,63,commit,(void *)42);select(&b2,MC_PATCH,MC_LEVEL_DEFAULT);hold(&b2,137);sb=request.seed;assert(sb!=sa);}
 puts("PASS controller metadata forwarding, target capture, zero seed rollover, level amounts on path 1, cancellation, invalid metadata/no callback refusal, retry, per-generator level session retention and init reset");return 0;}
