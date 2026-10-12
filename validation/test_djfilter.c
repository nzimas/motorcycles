/* DJ filter: off is bit-exact; low-pass passes lows and cuts highs, high-pass
   the reverse; resonance peak bounded; every position/resonance stable and
   silent after full-scale noise; no overflow (UBSan). */
#include "../firmware/djfilter.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double gain(int8_t pos,uint8_t res,double hz,double amp){
 mc_djfilter_state s;memset(&s,0,sizeof s);double in=0,out=0;
 for(unsigned b=0;b<600;b++){int32_t x[32],d[32];
  for(unsigned i=0;i<32;i++){double t=(b*32+i)/48000.0;x[i]=(int32_t)(amp*2147483647.0*sin(2*M_PI*hz*t));d[i]=x[i];}
  mc_djfilter_block(x,pos,res,&s);
  if(b>=300)for(unsigned i=0;i<32;i++){in+=(double)d[i]*d[i];out+=(double)x[i]*x[i];}}
 return 10*log10(out/in);
}
int main(void){unsigned cases=0;srand(7);
 {mc_djfilter_state s;memset(&s,0,sizeof s);int32_t x[32],y[32];for(unsigned i=0;i<32;i++)x[i]=y[i]=(int32_t)((unsigned)rand()<<16^(unsigned)rand());
  mc_djfilter_block(x,0,99,&s);assert(!memcmp(x,y,sizeof x));cases++;}
 double lp_low=gain(-40,0,80,0.25),lp_high=gain(-40,0,6000,0.25),hp_low=gain(40,0,80,0.25),hp_high=gain(40,0,6000,0.25);
 printf("LP-40: 80 Hz %.1f dB, 6 kHz %.1f dB | HP+40: 80 Hz %.1f dB, 6 kHz %.1f dB\n",lp_low,lp_high,hp_low,hp_high);
 assert(lp_low>-3&&lp_high<-25&&hp_low<-25&&hp_high>-3);cases++;
 /* Resonance: peak near cutoff bounded (and present). */
 {double peak=-99;for(double hz=200;hz<4000;hz*=1.05){double g=gain(-30,127,hz,0.05);if(g>peak)peak=g;}
  printf("max resonance peak (LP-30): %.1f dB\n",peak);assert(peak>6&&peak<20);cases++;}
 /* Stability: full-scale noise then silence decays to (near) zero everywhere. */
 for(int pos=-63;pos<=63;pos+=3)for(unsigned res=0;res<=127;res+=21){
  mc_djfilter_state s;memset(&s,0,sizeof s);int32_t x[32];long long tail=0;
  for(unsigned b=0;b<300;b++){for(unsigned i=0;i<32;i++)x[i]=(int32_t)((unsigned)rand()<<17^(unsigned)rand());mc_djfilter_block(x,(int8_t)pos,(uint8_t)res,&s);}
  for(unsigned b=0;b<3000;b++){memset(x,0,sizeof x);mc_djfilter_block(x,(int8_t)pos,(uint8_t)res,&s);if(b>=2900)for(unsigned i=0;i<32;i++)tail+=llabs((long long)x[i]);}
  assert(tail/3200<(1<<17));cases++;}  /* settles below -84 dBFS (16-bit design; measured worst -88.5) */
 /* Sweeps through off and across modes with random positions: no UB. */
 {mc_djfilter_state s;memset(&s,0,sizeof s);for(unsigned b=0;b<20000;b++){int32_t x[32];for(unsigned i=0;i<32;i++)x[i]=(int32_t)((unsigned)rand()<<17^(unsigned)rand());mc_djfilter_block(x,(int8_t)(rand()%127-63),(uint8_t)(rand()%256),&s);}cases++;}
 printf("PASS %u DJ filter cases: bypass bit-exact, LP/HP response, bounded resonance, stability\n",cases);return 0;}
