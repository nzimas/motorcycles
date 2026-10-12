/* LYRA voice (firmware/lyra.c): pitch accuracy, bounded output, low DC,
   knob behaviour, and sanitizer-clean integer arithmetic across all knobs. */
#include "../firmware/lyra.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static double run(int note,int sp,int sh,int mo,int hy,double *rms,double *dc,double *peak){
 lyra_state s;memset(&s,0,sizeof s);lyra_params p={note<<16,(uint8_t)sp,(uint8_t)sh,(uint8_t)mo,(uint8_t)hy};
 int32_t o[32];double s1=0,s2=0,pk=0;long zc=0;int prev=0;
 for(int b=0;b<1500;b++){lyra_block(&s,&p,o);for(int i=0;i<32;i++){double x=o[i]/2147483648.0;assert(fabs(x)<=0.5001);if(b>=300){s1+=x;s2+=x*x;if(fabs(x)>pk)pk=fabs(x);int g=o[i]>0;if(b>300&&g!=prev)zc++;prev=g;}}}
 double n=1200*32;*rms=sqrt(s2/n);*dc=s1/n;*peak=pk;return zc/2.0/(n/48000);}
int main(void){double r,d,p;unsigned cases=0;
 /* Pitch: clean voice (no spread, no mod) sounds the note. */
 for(int note=36;note<=84;note+=6){double f=run(note,0,0,0,0,&r,&d,&p),want=440*pow(2,(note-69)/12.0);assert(fabs(f-want)/want<0.01);assert(r>0.2);cases++;}
 /* Sharpness raises the level (triangle -> square). */
 {double r0,r1;run(57,0,0,0,0,&r0,&d,&p);run(57,0,127,0,0,&r1,&d,&p);assert(r1>r0*1.5);cases++;}
 /* Mod: clean at 0, chaotic (many more crossings) at 127. */
 {double f0=run(57,20,40,0,0,&r,&d,&p),f1=run(57,20,40,127,0,&r,&d,&p);assert(f1>f0*5);cases++;}
 /* Every knob combination on a coarse grid: bounded, sounding, DC small. */
 for(int sp=0;sp<128;sp+=21)for(int sh=0;sh<128;sh+=21)for(int mo=0;mo<128;mo+=21)for(int hy=0;hy<128;hy+=42)for(int note=20;note<=120;note+=50){
  run(note,sp,sh,mo,hy,&r,&d,&p);assert(fabs(d)<0.02);assert(r>0.02);cases++;}
 printf("PASS %u LYRA voice cases: pitch, sharpness, modulation, bounded output and DC\n",cases);return 0;}
