#include "music.h"
#include <assert.h>
#include <stdio.h>
int main(void){unsigned root,scale,n,tests=0;uint8_t packed,r,s,out=0xa5;
 assert(mc_music_unpack(0,&r,&s)&&r==9&&s==1);
 for(root=0;root<12;root++)for(scale=0;scale<4;scale++){
  assert(mc_music_pack(root,scale,&packed)&&mc_music_unpack(packed,&r,&s)&&r==root&&s==scale);
  for(n=0;n<128;n++){unsigned j,d;
   assert(mc_quantize_note(n,root,scale,0,127,&out));assert(mc_note_in_scale(out,root,scale));
   d=out>n?out-n:n-out;
   for(j=0;j<128;j++)if(mc_note_in_scale(j,root,scale)){unsigned dj=j>n?j-n:n-j;assert(dj>=d);if(dj==d)assert(out<=j);}
   assert(mc_quantize_note(n,root,scale,60,96,&out));assert(out>=60&&out<=96&&mc_note_in_scale(out,root,scale));tests++;
  }
 }
 out=0xa5;assert(!mc_music_unpack(0x8c,&r,&s));assert(!mc_music_unpack(0xc0,&r,&s));assert(!mc_music_unpack(1,&r,&s));
 assert(!mc_music_pack(12,0,&out)&&out==0xa5);assert(!mc_quantize_note(60,0,4,0,127,&out)&&out==0xa5);
 assert(!mc_quantize_note(61,0,2,61,61,&out)&&out==0xa5);
 assert(mc_retune_note(69,9,0,MC_NATURAL_MINOR,0,127,&out)&&out==72);
 assert(mc_retune_note(72,0,9,MC_NATURAL_MINOR,0,127,&out)&&out==69);
 assert(mc_retune_note(60,0,6,MC_MAJOR,0,127,&out)&&out==66);
 printf("PASS: %u scale/root/note nearest quantization cases; defaults, range, ties, shortest transpose, invalid atomicity\n",tests);
}
