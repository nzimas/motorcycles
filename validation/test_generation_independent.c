#include "../generation/generation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned bitcount(unsigned n){unsigned c=0;while(n){c+=n&1;n>>=1;}return c;}
int main(void){mc_request q={.generator=MC_PATTERN,.path=MC_ABSTRACT,.target=5,.density=30,.brutality=40,.machine_mask=63,.seed=123};mc_plan p,old;unsigned g,path,mask,d,i,j,cases=0;
 for(g=0;g<4;g++)for(path=0;path<4;path++)for(mask=1;mask<=63;mask++)for(d=0;d<=100;d+=25){
  q.generator=g;q.path=path;q.machine_mask=mask;q.density=d;q.seed=cases+1;if(g==MC_KIT&&path==MC_RHYTHMIC&&(!(mask&1)||!(mask&0x16)||!(mask&0x1c))){memset(&p,0xa5,sizeof p);old=p;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));cases++;continue;}assert(mc_generate(&q,&p));
  assert(p.sound_mask==(g==MC_PATCH?32:g==MC_KIT?63:0));
  /* Distortion record: amount on, bytes in range, reserved zero; a Kit stays
     within 6 x 30 instructions/sample of measured stage cost. */
  {unsigned cost=0;for(i=0;i<6;i++)if(p.sound_mask&(1u<<i)){const uint8_t *dr=p.extra[i].drive;assert(dr[0]>=1&&!dr[6]&&dr[7]<=127);  /* filter off, resonance set */for(j=0;j<6;j++)assert(dr[j]<=127);
    cost+=21+(dr[4]?9:0)+(dr[3]?9:0)+(dr[2]?24:dr[5]?18:0);}
   if(g==MC_KIT)assert(cost<=180);}
  /* Every generated track at full VOLUME+DIST: Patch/Kit sounds, Pattern all six. */
  assert(p.volume_mask==(g==MC_PATTERN?63:0));for(i=0;i<6;i++)if(p.sound_mask&(1u<<i))assert(p.extra[i].volume==127);
  for(i=0;i<6;i++)for(j=0;j<64;j++)for(unsigned k=0;k<p.phrase[i].step[j].lock_count;k++)assert(p.phrase[i].step[j].lock_k[k]!=19);assert(p.sequence_mask==(g==MC_PHRASE?32:g==MC_PATTERN?63:0));
  unsigned active=0;
  for(i=0;i<6;i++){
   if(p.sound_mask&(1u<<i)){uint8_t buffer[38],snapshot[38];memset(buffer,0xa5,38);memcpy(snapshot,buffer,38);assert(mask&(1u<<p.sound[i].machine));assert(mc_encode_engine(&p.sound[i],buffer,38));assert(!memcmp(buffer,snapshot,18));
    /* Extras touch exactly k1..8 and k19..22 of the sound words. */
    {uint8_t w[80],w0[80];memset(w,0x5a,80);memcpy(w0,w,80);assert(mc_encode_extras(&p.extra[i],w,80));
     for(unsigned k=0;k<40;k++){int own=(k>=1&&k<=8)||(k>=19&&k<=22),drive=k>=28&&k<=31;if(drive){assert(w[2*k]==p.extra[i].drive[2*(k-28)]&&w[2*k+1]==p.extra[i].drive[2*(k-28)+1]);}else if(!own){assert(w[2*k]==w0[2*k]&&w[2*k+1]==w0[2*k+1]);}else assert(w[2*k+1]==0);}
     assert(!mc_encode_extras(&p.extra[i],w,45));}}
   else {mc_sound empty={0};assert(!memcmp(&p.sound[i],&empty,sizeof empty));}
   unsigned events=0;for(j=0;j<64;j++){mc_event *e=&p.phrase[i].step[j];assert(e->on<=1&&e->note<=127&&e->velocity<=127);events+=e->on;if(e->on)assert(j<p.phrase[i].length);else if(e->trigless){assert(j<p.phrase[i].length&&e->lock_count&&!e->note&&!e->velocity&&!e->length&&!e->micro&&!e->retrig&&!e->condition);}else {mc_event empty={0};assert(!memcmp(e,&empty,sizeof empty));}}
   active+=events>0;
   if(!(p.sequence_mask&(1u<<i))){mc_phrase empty={0};assert(!memcmp(&p.phrase[i],&empty,sizeof empty));}
  }
  if(g<3)assert(active==(g<2?0:1));else{assert(active>=3&&active<=6);if(path==MC_RHYTHMIC){unsigned kick=0;for(j=0;j<64;j++)kick+=p.phrase[0].step[j].on;assert(kick);}}if(g==3)assert(p.tempo>=45&&p.tempo<=190);else assert(!p.tempo);
  assert(bitcount(p.sound_mask)<=6);cases++;
 }
 memset(&p,0xa5,sizeof p);old=p;q.generator=4;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));q.generator=0;q.path=4;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));q.path=0;q.machine_mask=0;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));q.machine_mask=128;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));q.machine_mask=63;q.density=101;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));q.density=50;q.brutality=101;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));assert(!mc_generate(0,&p));assert(!mc_generate(&q,0));
 {mc_sound sound={0,60,128,0,0,0,0,0,0,0};uint8_t buf[38],before[38];memset(buf,0xa5,38);memcpy(before,buf,38);assert(!mc_encode_engine(&sound,buf,38)&&!memcmp(buf,before,38));sound.color=0;assert(!mc_encode_engine(&sound,buf,37)&&!memcmp(buf,before,38));}
 printf("PASS %u independent generator/path/machine-mask/density cases and invalid-input atomicity\n",cases);return 0;
}
