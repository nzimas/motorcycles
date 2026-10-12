#include "../generation/generation.h"
#include "../generation/music.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mc_request q={.target=3,.density=80,.brutality=65,.machine_mask=255,.seed=113};mc_plan p,old;unsigned checks=0;
 assert(sizeof(q)==88&&offsetof(mc_request,default_note)==80&&offsetof(mc_request,note_valid_mask)==86);
 for(unsigned root=0;root<12;root++)for(unsigned scale=0;scale<4;scale++){
  assert(mc_music_pack(root,scale,&q.music));q.note_valid_mask=63;
  for(unsigned note=36;note<=96;note++){
   for(unsigned t=0;t<6;t++)q.default_note[t]=note;
   for(unsigned g=0;g<2;g++){q.generator=g;q.path=MC_FREE;assert(mc_generate(&q,&p));for(unsigned t=0;t<6;t++)if(p.sound_mask&(1u<<t)){int effective=(int)p.sound[t].pitch+(int)note-64;assert((effective%12+12)%12==(int)root);assert(p.sound[t].finetune==64);}checks++;}
  }
  q.source_sound_valid_mask=q.machine_valid_mask=63;for(unsigned t=0;t<6;t++){q.current_machine[t]=4;q.current_sound[t]=(mc_sound){.machine=4,.pitch=40+t*8,.finetune=64,.decay=60};}
  for(unsigned g=2;g<4;g++)for(unsigned path=0;path<4;path++){q.generator=g;q.path=path;assert(mc_generate(&q,&p));for(unsigned t=0;t<6;t++)for(unsigned i=0;i<64;i++)if(p.phrase[t].step[i].on){int effective=(int)p.phrase[t].step[i].note+(int)q.current_sound[t].pitch-64;assert(mc_note_in_scale((effective%12+12)%12,root,scale));}checks++;}
  q.source_sound_valid_mask=q.machine_valid_mask=0;
 }
 memset(&p,0xa5,sizeof p);old=p;q.note_valid_mask=64;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));q.note_valid_mask=1;q.default_note[0]=128;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));
 q.generator=MC_PATCH;q.machine_mask=1;q.machine_valid_mask=q.source_sound_valid_mask=0;q.note_valid_mask=8;q.default_note[3]=127;q.music=0x99;memset(&p,0xa5,sizeof p);old=p;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));
 printf("PASS %u independent88B custom pad/default note and effective source pitch musical context cases\n",checks);return 0;}
