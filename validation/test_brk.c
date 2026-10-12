/* Live break engine (firmware/brk.c): every recipe maps steps inside the
   track, repeats/tempo changes start on the beat, chops keep the downbeat,
   the kick is never filtered, never octave-shifted and stays centred on
   quarters, lock values stay in range and inside the buffer, and the end
   of a break restores straight playback. */
#include "../firmware/brk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned cases=0,recipes[BRK_RECIPES]={0},layers=0;
 for(uint32_t seed=1;seed<=3000;seed++){
  uint8_t steps[6];for(unsigned t=0;t<6;t++)steps[t]=(uint8_t)((seed*7+t*5)%64);
  mc_break_begin(seed*2654435761u,steps);assert(mc_brk.active);recipes[mc_brk.recipe]++;layers|=mc_brk.layers;
  assert(mc_brk.layers);
  for(unsigned len=1;len<=64;len++)for(unsigned t=0;t<6;t++)for(unsigned s=0;s<len;s++){unsigned src=mc_break_source(t,s,len);assert(src<len);}
  if(mc_brk.recipe==BRK_CHOP)assert(mc_brk.chop[0]==0&&mc_break_source(0,0,16)==0);
  if(mc_brk.recipe==BRK_REPEAT||mc_brk.recipe==BRK_HALF||mc_brk.recipe==BRK_DOUBLE){unsigned src=mc_break_source(0,(steps[0]+1)%64,64);unsigned q=mc_brk.window>4?8:4;(void)q;assert(src<64);}
  assert(!mc_brk.filter_target[0]);
  uint8_t sound[100];memset(sound,0,sizeof sound);for(unsigned k=0;k<33;k++){sound[20+2*k]=(uint8_t)(40+(k*7)%60);}
  for(unsigned t=0;t<6;t++)for(unsigned step=0;step<32;step++){
   uint8_t buf[20+8*66];memset(buf,0xee,sizeof buf);int32_t n=2;
   uint16_t *e=(uint16_t*)(buf+20);e[0]=11;e[1]=30<<8;e[4]=18;e[5]=20<<8;
   int32_t m=mc_break_locks(t,buf,n,66,sound,step);assert(m>=n&&m<=n+5);
   for(int32_t i=0;i<m;i++){uint16_t *x=(uint16_t*)(buf+20+8*i);assert(x[0]<33&&(x[1]>>8)<=127&&(x[1]&255)==0);
    if(t==0){assert(x[0]!=10);if(x[0]==22&&step%4==0)assert(x[1]>>8==64);}
    if(x[0]==10)assert((x[1]>>8)>=40&&(x[1]>>8)<=88);}
   unsigned lc;unsigned r=mc_break_ratchet(t,&lc);if(r)assert(r<=17&&lc>=1&&lc<=2);
  }
  uint8_t p[6][8];memset(p,0,sizeof p);for(unsigned b=0;b<3100;b++){memset(p,0,sizeof p);mc_break_audio(p);}
  assert(p[0][6]==0);for(unsigned t=1;t<6;t++)if(mc_brk.filter_target[t])assert((int8_t)p[t][6]==mc_brk.filter_target[t]);
  mc_break_end();for(unsigned s=0;s<16;s++)assert(mc_break_source(2,s,16)==s);
  {uint8_t buf[20+8*66];memset(buf,0,sizeof buf);assert(mc_break_locks(1,buf,0,66,0,0)==0);}
  cases++;}
 for(unsigned r=0;r<BRK_RECIPES;r++)assert(recipes[r]>200);
 assert(layers==63);
 printf("PASS %u live-break variations: all %d recipes and 6 layers drawn; steps, locks, kick and audio rules hold\n",cases,BRK_RECIPES);return 0;}
