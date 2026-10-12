#include "../interface/interface.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>
static unsigned calls,failures;static mc_ui_request last;static int result=1;
static int publish(void *p,const mc_ui_request *r){(void)p;calls++;last=*r;return result;}
static void check(int yes,const char *name){printf("%s %s\n",yes?"PASS":"FAIL",name);failures+=!yes;}
static int ev(mc_interface*s,unsigned e,unsigned i,int v,unsigned target,uint32_t t){return mc_event_handle(s,e,i,v,target,t,publish,0);}
/* PAGE + pad g, then release PAGE: the generator stays latched. */
static void enter(mc_interface*s,unsigned g,unsigned target,uint32_t t){mc_init(s);ev(s,MC_PAGE,0,1,target,t);ev(s,MC_PAD,g,1,target,t);ev(s,MC_PAD,g,0,target,t);ev(s,MC_PAGE,0,0,target,t);}
static void level(mc_interface*s,unsigned l,uint32_t t){ev(s,MC_STEP,l-1,1,0,t);ev(s,MC_STEP,l-1,0,0,t);}
static uint16_t bar(unsigned l){return (uint16_t)((1u<<l)-1u);}
int main(void){mc_interface s;unsigned n,ok;uint32_t t;
 /* Entry */
 enter(&s,0,4,0);check(s.state==MC_READY&&s.generator==0&&s.path==1&&s.target==4&&!s.page,"PAGE+pad enters latched READY on captured track, path 1");
 ok=1;for(n=0;n<4;n++)ok&=s.level[n]==MC_LEVEL_DEFAULT;check(ok&&MC_LEVEL_DEFAULT==7,"default level 7 for every generator");
 /* Step 16 hold timing */
 calls=0;check(ev(&s,MC_STEP,15,1,1,10)&&s.state==MC_HOLD,"step16 press consumed and starts hold");
 ev(&s,MC_TICK,0,0,1,1509);check(!calls&&s.state==MC_HOLD,"1499 ms never generates");
 ev(&s,MC_TICK,0,0,1,1510);check(calls==1&&last.target==4&&last.generator==0&&last.path==1&&last.music==0,"1500 ms generates once on captured target, path 1, music 0");
 check(last.density==mc_level_amount(7)&&last.brutality==mc_level_amount(7)&&last.density==42,"request density=brutality=level amount (level 7 -> 42)");
 check(s.state==MC_READY&&s.acknowledgement==1,"successful publication acknowledged, back to READY");
 ev(&s,MC_STEP,15,1,2,1600);ev(&s,MC_TICK,0,0,2,5000);check(calls==1,"duplicate press/continued hold do not retrigger");
 check(ev(&s,MC_STEP,15,0,2,5001),"step16 release consumed");ev(&s,MC_STEP,15,1,2,5100);ev(&s,MC_TICK,0,0,2,6599);check(calls==1,"fresh hold 1499 ms does not generate");ev(&s,MC_TICK,0,0,2,6600);check(calls==2&&last.target==4,"release and fresh hold retrigger at 1500 ms");
 ev(&s,MC_STEP,15,0,2,6700);
 /* Short release does nothing; ticks after release do nothing. */
 ev(&s,MC_STEP,15,1,0,7000);ev(&s,MC_STEP,15,0,0,8000);ev(&s,MC_TICK,0,0,0,9000);check(calls==2&&s.state==MC_READY&&!s.audition,"release before 1500 ms does nothing");
 /* Knobs keep their ordinary function in generator mode. */
 n=s.level[0];check(ev(&s,MC_DENSITY,0,INT_MAX,0,9100)==0&&ev(&s,MC_BRUTALITY,0,INT_MIN,0,9101)==0&&s.level[0]==n,"Density/Brutality knobs not consumed in generator mode");
 check(ev(&s,MC_TICK,0,0,0,9102)==0,"TICK never consumed");
 /* Levels on steps 1..15 */
 ok=1;for(n=1;n<=15;n++){ok&=ev(&s,MC_STEP,n-1,1,0,10000+n)==1;ok&=ev(&s,MC_STEP,n-1,0,0,10000+n)==1;ok&=s.level[0]==n;ok&=mc_leds(&s,10400)==(bar(n)|0x8000)&&mc_leds(&s,10800)==bar(n);}
 check(ok,"steps 1..15 set level 1..15, press/release consumed, LED bar steps 1..level");
 check((mc_leds(&s,10400)&0x8000)&&(mc_leds(&s,10799)&0x8000)&&!(mc_leds(&s,10800)&0x8000)&&!(mc_leds(&s,11199)&0x8000)&&(mc_leds(&s,11200)&0x8000),"step16 LED blinks with 400 ms period when idle");
 calls=0;level(&s,1,11000);ev(&s,MC_STEP,15,1,0,11001);ev(&s,MC_TICK,0,0,0,12501);check(calls==1&&last.density==0&&last.brutality==0,"level 1 requests amount 0");ev(&s,MC_STEP,15,0,0,12502);
 level(&s,15,13000);ev(&s,MC_STEP,15,1,0,13001);ev(&s,MC_TICK,0,0,0,14501);check(calls==2&&last.density==100&&last.brutality==100,"level 15 requests amount 100");ev(&s,MC_STEP,15,0,0,14502);
 level(&s,8,15000);ev(&s,MC_STEP,15,1,0,15001);ev(&s,MC_TICK,0,0,0,16501);check(calls==3&&last.density==50&&last.brutality==50,"level 8 requests amount 50");ev(&s,MC_STEP,15,0,0,16502);
 ok=mc_level_amount(0)==0&&mc_level_amount(1)==0&&mc_level_amount(15)==100&&mc_level_amount(16)==100&&mc_level_amount(255)==100;for(n=1;n<15;n++)ok&=mc_level_amount(n)<mc_level_amount(n+1)&&mc_level_amount(n)==(n-1)*100/14;check(ok,"mc_level_amount maps 1..15 to 0..100, clamped");
 ev(&s,MC_STEP,3,1,0,17000);check(ev(&s,MC_STEP,3,1,0,17001)==1&&s.level[0]==4,"duplicate level press consumed");ev(&s,MC_STEP,3,0,0,17002);check(ev(&s,MC_STEP,3,0,0,17003)==1,"stray level release in generator mode consumed");
 /* LEDs while step 16 is held: 100 ms blink */
 enter(&s,1,0,0);level(&s,5,0);ev(&s,MC_STEP,15,1,0,1000);check(mc_leds(&s,1000)==(bar(5)|0x8000)&&mc_leds(&s,1100)==bar(5)&&mc_leds(&s,1200)==(bar(5)|0x8000)&&mc_leds(&s,1350)==bar(5),"step16 LED blinks at 100 ms while held");
 ev(&s,MC_STEP,15,0,0,1400);check(mc_leds(&s,1500)==bar(5)&&mc_leds(&s,1600)==(bar(5)|0x8000),"step16 LED returns to 400 ms after release");
 /* Per-generator level memory */
 enter(&s,0,2,0);level(&s,3,10);ev(&s,MC_PAGE,0,1,0,20);ev(&s,MC_PAD,2,1,0,21);ev(&s,MC_PAD,2,0,0,22);ev(&s,MC_PAGE,0,0,0,23);
 check(s.generator==2&&s.level[2]==7&&s.target==2&&(mc_leds(&s,0)&0x7fff)==bar(7),"switching generator shows its own default level, retains target");
 level(&s,12,30);ev(&s,MC_PAD,0,1,0,40);ev(&s,MC_PAD,0,0,0,41);check(s.generator==0&&s.level[0]==3&&s.level[2]==12&&(mc_leds(&s,0)&0x7fff)==bar(3),"levels remembered per generator");
 calls=0;ev(&s,MC_STEP,15,1,0,50);ev(&s,MC_TICK,0,0,0,1550);check(calls==1&&last.generator==0&&last.density==mc_level_amount(3),"request uses the current generator's level");ev(&s,MC_STEP,15,0,0,1551);
 ev(&s,MC_PAGE,0,1,0,1600);ev(&s,MC_PAGE,0,0,0,1601);check(s.state==MC_INACTIVE&&s.level[0]==3&&s.level[2]==12,"levels survive PAGE exit");
 ev(&s,MC_PAGE,0,1,5,1700);ev(&s,MC_PAD,2,1,5,1701);ev(&s,MC_PAD,2,0,5,1702);ev(&s,MC_PAGE,0,0,5,1703);check(s.level[2]==12&&s.target==5&&(mc_leds(&s,0)&0x7fff)==bar(12),"re-entry restores remembered level");
 mc_init(&s);check(s.level[0]==7&&s.level[1]==7&&s.level[2]==7&&s.level[3]==7,"mc_init resets levels");
 /* PAGE cancel and ownership */
 enter(&s,0,3,0);calls=0;ev(&s,MC_STEP,15,1,0,5);ev(&s,MC_PAGE,0,1,0,1099);ev(&s,MC_PAGE,0,0,0,1100);ev(&s,MC_TICK,0,0,0,2000);check(!calls&&s.state==MC_INACTIVE&&mc_leds(&s,2000)==0,"PAGE cancels hold and restores LED ownership");
 check(ev(&s,MC_STEP,15,0,0,2001),"owned step16 release consumed after PAGE");check(!ev(&s,MC_STEP,15,0,0,2002),"duplicate release returns to ordinary dispatch");
 enter(&s,0,3,0);ev(&s,MC_STEP,3,1,0,5);ev(&s,MC_PAGE,0,1,0,6);ev(&s,MC_PAGE,0,0,0,7);check(s.state==MC_INACTIVE&&s.level[0]==4,"level press then PAGE exit");
 check(ev(&s,MC_STEP,3,0,0,8)==1,"owned level release consumed after PAGE exit");check(ev(&s,MC_STEP,3,0,0,9)==0&&ev(&s,MC_STEP,3,1,0,10)==0&&ev(&s,MC_STEP,3,0,0,11)==0,"later inactive step press/release go to ordinary dispatch");
 check(ev(&s,MC_DENSITY,0,10,0,12)==0&&ev(&s,MC_STEP,15,1,0,13)==0&&ev(&s,MC_STEP,15,0,0,14)==0,"inactive knobs/steps not consumed");
 /* A level key held while switching to the scale menu, released there, must not
    leave stale ownership that swallows a later ordinary release. */
 enter(&s,0,1,0);ev(&s,MC_STEP,2,1,0,10);ev(&s,MC_PAGE,0,1,0,20);ev(&s,MC_PAD,4,1,0,21);ev(&s,MC_PAD,4,0,0,22);ev(&s,MC_PAGE,0,0,0,23);
 check(s.state==MC_SCALE&&ev(&s,MC_STEP,2,0,0,30)==1,"level release in scale menu consumed");ev(&s,MC_PAGE,0,1,0,40);ev(&s,MC_PAGE,0,0,0,41);
 n=ev(&s,MC_STEP,2,1,0,50);check(s.state==MC_INACTIVE&&n==0&&ev(&s,MC_STEP,2,0,0,60)==0,"no stale level ownership after scale menu: later ordinary press/release both dispatched");
 /* Generator switch cancels hold */
 enter(&s,0,5,0);calls=0;ev(&s,MC_STEP,15,1,0,10);ev(&s,MC_PAD,3,1,0,100);check(s.state==MC_READY&&s.generator==3&&s.target==5,"generator switch cancels hold and retains target");ev(&s,MC_TICK,0,0,0,2000);check(!calls,"cancelled hold never generates");check(ev(&s,MC_STEP,15,0,0,2001),"release of cancelled hold consumed");
 ev(&s,MC_STEP,15,1,0,2100);ev(&s,MC_TICK,0,0,0,3600);check(calls==1&&last.generator==3&&last.target==5,"new generator generates on captured target");
 /* uint32 wrap */
 enter(&s,2,0,UINT_MAX-400);calls=0;ev(&s,MC_STEP,15,1,0,UINT_MAX-300);ev(&s,MC_TICK,0,0,0,1198);check(!calls,"hold 1499 ms over uint32 wrap does not generate");ev(&s,MC_TICK,0,0,0,1199);check(calls==1&&last.generator==2,"hold timer works over uint32 wrap");
 /* Pads */
 mc_init(&s);ev(&s,MC_PAGE,0,1,1,0);ev(&s,MC_PAD,0,1,1,1);ev(&s,MC_PAGE,0,0,1,2);check(s.state==MC_READY,"PAGE release after entering keeps mode latched");ev(&s,MC_PAGE,0,1,1,3);ev(&s,MC_PAGE,0,0,1,4);check(s.state==MC_INACTIVE,"later PAGE tap exits");check(ev(&s,MC_PAD,0,0,1,5),"owned generator pad release consumed after PAGE");
 mc_init(&s);ev(&s,MC_PAGE,0,1,6,0);check(ev(&s,MC_PAD,0,1,6,1)&&s.state==MC_INACTIVE,"invalid track refuses entry");ev(&s,MC_PAGE,0,0,6,2);
 /* Acknowledgement LEDs: success sweep then flashes */
 enter(&s,3,0,0);level(&s,2,1);calls=0;ev(&s,MC_STEP,15,1,0,2);ev(&s,MC_TICK,0,0,0,1502);check(calls==1&&s.acknowledgement==1,"generation succeeds");
 ok=1;for(t=0;t<400;t++){unsigned k=1+t*16/400;ok&=mc_leds(&s,1502+t)==(uint16_t)((1u<<k)-1u);}check(ok&&mc_leds(&s,1502)==1&&mc_leds(&s,1502+399)==0xffff,"success sweep lights steps 1..n over 400 ms");
 ok=1;for(t=400;t<800;t++)ok&=mc_leds(&s,1502+t)==(((t-400)/100)%2?0:0xffff);check(ok&&mc_leds(&s,1902)==0xffff&&mc_leds(&s,2002)==0&&mc_leds(&s,2102)==0xffff&&mc_leds(&s,2202)==0,"success then flashes all 16 every 100 ms, on first");
 check(mc_leds(&s,2302)==(bar(2)|((2302/400)%2?0:0x8000)),"after 800 ms LEDs return to level bar");
 result=0;ev(&s,MC_STEP,15,0,0,2303);ev(&s,MC_STEP,15,1,0,2304);ev(&s,MC_TICK,0,0,0,3804);check(calls==2&&s.acknowledgement==2&&s.state==MC_READY,"publication failure gives distinct status and returns ready");
 ok=1;for(t=0;t<800;t++){uint32_t x=3804+t;ok&=mc_leds(&s,x)==((x/200)%2?0xaaaa:0x5555);}check(ok,"failure alternates 0xaaaa/0x5555 every 200 ms for 800 ms");
 check(mc_leds(&s,4604)==(bar(2)|((4604/400)%2?0:0x8000)),"failure display ends after 800 ms");
 ev(&s,MC_STEP,15,0,0,4700);ev(&s,MC_STEP,15,1,0,4701);ev(&s,MC_TICK,0,0,0,6201);check(calls==3,"retry after failure generates on fresh hold");result=1;
 /* Acknowledgement wrap */
 enter(&s,1,0,UINT_MAX-2000);ev(&s,MC_STEP,15,1,0,UINT_MAX-1600);ev(&s,MC_TICK,0,0,0,UINT_MAX-100);check(mc_leds(&s,UINT_MAX-100)==1&&mc_leds(&s,299)==0xffff&&mc_leds(&s,699)==(bar(7)|((699/400)%2?0:0x8000)),"acknowledgement timing across uint32 wrap");
 printf("%u independent failures\n",failures);return failures?1:0;}
