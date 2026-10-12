#include "sequence_encoding.h"
#include <stddef.h>
extern void *memcpy(void *,const void *,size_t);
static int overlaps(uintptr_t a,unsigned na,uintptr_t b,unsigned nb){return a<=b?b-a<na:a-b<nb;}
static void put_word(uint8_t *p,unsigned x){p[0]=(uint8_t)(x>>8);p[1]=(uint8_t)x;}
/* Lockable sound words: LFO 1..8 except destination 4, Pitch..Pan 10..22. */
static int lockable(unsigned k){return k>=1&&k<=22&&k!=4&&k!=9;}
static unsigned lock_max(unsigned k){return k==2?23:k==5?6:k==7?4:k==10?88:k==15||k==16?1:127;}
int mc_stage_sequence(const mc_phrase *phrase,const uint8_t *old_track,
 uint8_t *new_track,uint8_t *new_locks,const uint8_t codes[13],unsigned update_length){
 unsigned i,j;uint8_t default_note;uint64_t used=0;uintptr_t ta=(uintptr_t)new_track,la=(uintptr_t)new_locks,oa=(uintptr_t)old_track,pa=(uintptr_t)phrase,ca=(uintptr_t)codes;
 if(!phrase||!old_track||!new_track||!new_locks||!codes||update_length>1||
    ta>UINTPTR_MAX-MC_TRACK_BYTES||la>UINTPTR_MAX-MC_LOCK_BYTES||oa>UINTPTR_MAX-MC_TRACK_BYTES||pa>UINTPTR_MAX-sizeof *phrase||ca>UINTPTR_MAX-13||
    overlaps(ta,MC_TRACK_BYTES,la,MC_LOCK_BYTES)||
    (old_track!=new_track&&overlaps(oa,MC_TRACK_BYTES,ta,MC_TRACK_BYTES))||overlaps(oa,MC_TRACK_BYTES,la,MC_LOCK_BYTES)||
    overlaps(pa,sizeof *phrase,ta,MC_TRACK_BYTES)||overlaps(pa,sizeof *phrase,la,MC_LOCK_BYTES)||
    overlaps(ca,13,ta,MC_TRACK_BYTES)||overlaps(ca,13,la,MC_LOCK_BYTES))return 0;
 if(phrase->length>64)return 0;
 for(i=0;i<64;i++){
  const mc_event *e=&phrase->step[i];
  if(e->on>1)return 0;
  if(e->on&&(i>=phrase->length||e->note>127||!e->velocity||e->velocity>127||!e->length||e->length>12||codes[e->length]>126))return 0;
  if(e->trigless>1||(e->trigless&&(e->on||i>=phrase->length||!e->lock_count)))return 0;
  if(!e->on&&(e->micro||e->retrig||e->condition))return 0;
  if(e->micro<-23||e->micro>23||e->retrig>17||e->condition>64)return 0;
  if(e->retrig&&(!e->retrig_length||e->retrig_length>12||codes[e->retrig_length]>126))return 0;
  if(e->lock_count>MC_STEP_LOCKS||(e->lock_count&&!e->on&&!e->trigless))return 0;
  for(j=0;j<e->lock_count;j++){
   unsigned k=e->lock_k[j],v=e->lock_v[j],m;
   if(!lockable(k)||v>lock_max(k)||(k==10&&v<40))return 0;
   for(m=0;m<j;m++)if(e->lock_k[m]==k)return 0;
  }
 }
 default_note=old_track[712];
 if(new_track!=old_track)memcpy(new_track,old_track,MC_TRACK_BYTES);
 /* Actual stock initializer40061480 baseline, not memset(-1) guesses. */
 for(i=0;i<64;i++){
  const mc_event *e=&phrase->step[i];
  put_word(new_track+2*i,i&1?0x10:0);
  new_track[128+i]=255;new_track[192+i]=255;new_track[256+i]=0;
  new_track[320+i]=46;new_track[384+i]=9;new_track[448+i]=0;
  new_track[516+i]=255;new_track[580+i]=default_note;new_track[644+i]=255;
  for(j=0;j<33;j++)put_word(new_locks+68*i+2*j,65535);
  put_word(new_locks+68*i+66,0);
  if(e->on){
   /* Note trig; bit15 enables retrig. */
   put_word(new_track+2*i,(i&1?0x10:0)|0x881|(e->retrig?0x8000:0));
   new_track[128+i]=e->velocity;new_track[192+i]=codes[e->length];new_track[580+i]=e->note;
   new_track[256+i]=(uint8_t)e->micro;
   if(e->retrig){new_track[384+i]=(uint8_t)(e->retrig-1);new_track[320+i]=codes[e->retrig_length];new_track[448+i]=(uint8_t)e->retrig_fade;}
   if(e->condition)new_track[516+i]=(uint8_t)(e->condition-1);
  }else if(e->trigless)put_word(new_track+2*i,(i&1?0x10:0)|0x6801); /* stock lock trig shape */
  /* Locks: word k holds value<<8; +66 counts the step's locks; +4352+k flags
     the slot as used on this track, which is all the saver looks at. */
  for(j=0;j<e->lock_count;j++){put_word(new_locks+68*i+2*e->lock_k[j],(unsigned)e->lock_v[j]<<8);used|=UINT64_C(1)<<e->lock_k[j];}
  put_word(new_locks+68*i+66,e->lock_count);
 }
 for(i=4352;i<4385;i++)new_locks[i]=(uint8_t)((used>>(i-4352))&1);
 new_track[719]=100; /* Verified stock chance:100 unconditionally fires. */
 if(update_length&&phrase->length)put_word(new_track+713,phrase->length);
 return 1;
}
unsigned mc_lock_pairs(const uint8_t *locks){unsigned k,n=0;for(k=0;k<33;k++)n+=locks[4352+k]!=0;return n;}
