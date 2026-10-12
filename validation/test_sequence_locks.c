/* Phrase/Pattern articulation: parameter locks, lock-only steps, microtiming,
   retrigs and conditions are valid, staged byte-exactly, inside the 79-pair
   save limit, key-safe, kick-safe, and independent of Brutality for events. */
#include "../generation/generation.h"
#include "../generation/sequence_encoding.h"
#include "../generation/music.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t track[MC_TRACK_BYTES],locks[MC_LOCK_BYTES];
static unsigned events(const mc_phrase *p){unsigned n=0;for(unsigned i=0;i<64;i++)n+=p->step[i].on;return n;}
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
int main(void){
 uint8_t codes[13];for(unsigned i=0;i<13;i++)codes[i]=(uint8_t)(10*i);
 mc_request q={.target=2,.machine_mask=255,.machine_valid_mask=63,.source_sound_valid_mask=63,.note_valid_mask=63};
 static const uint8_t machines[6]={0,1,2,3,4,MC_MACRO};
 for(unsigned t=0;t<6;t++){q.current_machine[t]=machines[t];q.current_sound[t]=(mc_sound){machines[t],64,40,machines[t]==MC_MACRO?41:40,40,40,0,0,64,60};q.default_note[t]=60;}
 q.current_sound[0].pitch=70;
 mc_plan p,low,high;unsigned cases=0,locked_steps=0,trigless=0,retrigs=0,conds=0,micro=0,pitchlocks=0,lockslots=0;
 for(unsigned g=MC_PHRASE;g<=MC_PATTERN;g++)for(unsigned path=0;path<4;path++)for(unsigned d=0;d<=100;d+=20)for(unsigned b=0;b<=100;b+=50)for(unsigned seed=1;seed<=24;seed++){
  q.generator=g;q.path=path;q.density=d;q.brutality=b;q.seed=seed*2654435761u;q.locks_elsewhere=(uint8_t)(g==MC_PHRASE?(seed*7)%90:0);
  assert(mc_music_pack(seed%12,seed%4,&q.music));
  assert(mc_generate(&q,&p));
  unsigned pairs=0;uint8_t tonic,scale;assert(mc_music_unpack(q.music,&tonic,&scale));
  for(unsigned t=0;t<6;t++){
   if(!(p.sequence_mask&(1u<<t)))continue;
   const mc_phrase *ph=&p.phrase[t];
   memset(track,0x5a,sizeof track);track[712]=60;memset(locks,0xa5,sizeof locks);
   assert(mc_stage_sequence(ph,track,track,locks,codes,1));
   uint64_t used=0;
   for(unsigned i=0;i<64;i++){
    const mc_event *e=&ph->step[i];unsigned flags=word(track+2*i),n=0;
    if(e->on)assert((flags&0x7fff)==((i&1?0x10u:0)|0x881)&&((flags>>15)==(e->retrig!=0)));
    else if(e->trigless)assert(flags==((i&1?0x10u:0)|0x6801));
    else assert(flags==(i&1?0x10u:0));
    assert((int8_t)track[256+i]==e->micro);
    assert(track[516+i]==(e->condition?e->condition-1:255));
    if(e->retrig)assert(track[384+i]==e->retrig-1&&track[320+i]==codes[e->retrig_length]&&(int8_t)track[448+i]==e->retrig_fade);
    for(unsigned k=0;k<33;k++){unsigned w=word(locks+68*i+2*k);if(w!=0xffff){n++;assert(w<0x8000&&!(w&255));used|=1ull<<k;}}
    assert(n==e->lock_count&&word(locks+68*i+66)==n);
    for(unsigned j=0;j<e->lock_count;j++){
     unsigned k=e->lock_k[j],v=e->lock_v[j];assert(k!=4&&k!=9&&k>=1&&k<=22);
     if(k==10){assert(v>=40&&v<=88);pitchlocks++;
      /* Played pitch stays in key: note + lock - 64 against the tonic. */
      unsigned note=e->on?e->note:60;int eff=(int)note+(int)v-64;assert(eff>=0&&mc_note_in_scale((unsigned)eff,tonic,scale));
      assert(!(t==0&&q.current_machine[0]==0));}
     if(k==12&&q.current_machine[t]==MC_MACRO)assert(v<=46);
    }
    locked_steps+=e->lock_count>0;trigless+=e->trigless;retrigs+=e->retrig!=0;conds+=e->condition!=0;micro+=e->micro!=0;
    if(t==0&&e->on)assert(!e->condition&&(path==MC_ABSTRACT||path==MC_FREE||!e->micro)&&(path!=MC_RHYTHMIC||b>=60||!e->retrig)); /* kick track: never conditional; on grid except Abstract/Free */
   }
   for(unsigned k=0;k<33;k++)assert(locks[4352+k]==((used>>k)&1));
   assert(mc_lock_pairs(locks)==(unsigned)__builtin_popcountll(used));pairs+=mc_lock_pairs(locks);lockslots+=mc_lock_pairs(locks);
  }
  /* Pattern owns all 79 pairs; Phrase stays within what other tracks leave. */
  if(g==MC_PATTERN)assert(pairs<=MC_LOCK_PAIRS);else assert(pairs+(q.locks_elsewhere>=MC_LOCK_PAIRS?MC_LOCK_PAIRS:q.locks_elsewhere)<=MC_LOCK_PAIRS);
  if(g==MC_PHRASE&&q.locks_elsewhere>=MC_LOCK_PAIRS)assert(pairs==0);
  cases++;
 }
 /* Brutality never changes how many events a sequence has, on any path. */
 for(unsigned g=MC_PHRASE;g<=MC_PATTERN;g++)for(unsigned path=0;path<4;path++)for(unsigned seed=1;seed<=64;seed++){
  q.generator=g;q.path=path;q.density=55;q.seed=seed;q.locks_elsewhere=0;q.brutality=0;assert(mc_generate(&q,&low));q.brutality=100;assert(mc_generate(&q,&high));
  for(unsigned t=0;t<6;t++)assert(events(&low.phrase[t])==events(&high.phrase[t]));
  if(path==MC_ABSTRACT||path==MC_MELODIC)assert(!memcmp(&low,&high,sizeof low));
  cases++;
 }
 /* Invalid lock content is rejected atomically by the stager. */
 {mc_phrase ph={.length=16};uint8_t t0[MC_TRACK_BYTES],l0[MC_LOCK_BYTES];memset(track,1,sizeof track);memset(locks,2,sizeof locks);memcpy(t0,track,sizeof t0);memcpy(l0,locks,sizeof l0);
  ph.step[0]=(mc_event){.on=1,.note=60,.velocity=100,.length=1,.lock_count=1,.lock_k={9},.lock_v={1}};assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].lock_k[0]=4;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].lock_k[0]=10;ph.step[0].lock_v[0]=39;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].lock_v[0]=89;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].lock_k[0]=11;ph.step[0].lock_v[0]=128;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].lock_v[0]=100;ph.step[0].micro=24;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].micro=0;ph.step[0].retrig=18;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].retrig=0;ph.step[0].condition=65;assert(!mc_stage_sequence(&ph,track,track,locks,codes,1));
  ph.step[0].condition=0;ph.step[1]=(mc_event){.trigless=1};assert(!mc_stage_sequence(&ph,track,track,locks,codes,1)); /* lock-only step without locks */
  assert(!memcmp(track,t0,sizeof t0)&&!memcmp(locks,l0,sizeof l0));cases++;}
 assert(locked_steps&&trigless&&retrigs&&conds&&micro&&pitchlocks&&lockslots);
 printf("PASS %u sequence articulation cases: %u locked steps, %u lock-only steps, %u retrigs, %u conditions, %u microtimed, %u pitch locks\n",cases,locked_steps,trigless,retrigs,conds,micro,pitchlocks);
 return 0;}
