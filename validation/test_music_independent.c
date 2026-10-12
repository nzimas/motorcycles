#include "../generation/music.h"
#include <assert.h>
#include <stdio.h>
int main(void){static const unsigned masks[4]={0xab5,0x5ad,0x555,0x18d};unsigned checks=0;uint8_t root,scale,out;
 for(unsigned b=0;b<256;b++){root=scale=0xa5;int ok=mc_music_unpack(b,&root,&scale);int expected=!b||((b&128)&&!(b&64)&&(b&15)<12);assert(ok==expected);if(!ok)assert(root==0xa5&&scale==0xa5);else if(!b)assert(root==9&&scale==1);checks++;}
 for(unsigned r=0;r<12;r++)for(unsigned s=0;s<4;s++)for(unsigned n=0;n<128;n++){
  unsigned pc=(n+12-r)%12;assert(mc_note_in_scale(n,r,s)==!!(masks[s]&(1u<<pc)));out=0xa5;assert(mc_quantize_note(n,r,s,40,96,&out));assert(out>=40&&out<=96&&(masks[s]&(1u<<((out+12-r)%12))));
  for(unsigned k=40;k<=96;k++)if(masks[s]&(1u<<((k+12-r)%12))){unsigned d=k>n?k-n:n-k,best=out>n?out-n:n-out;assert(d>=best);if(d==best)assert(k>=out);}checks++;
 }
 out=0xa5;assert(!mc_music_pack(12,0,&out)&&out==0xa5);assert(!mc_quantize_note(60,0,4,40,96,&out)&&out==0xa5);assert(!mc_quantize_note(60,0,0,61,61,&out)&&out==0xa5);
 assert(mc_retune_note(60,0,6,0,40,96,&out)&&out==66);assert(mc_retune_note(60,0,11,0,40,96,&out)&&out==59);
 printf("PASS %u independent packing, pitchclass, nearest/tie and transposition cases\n",checks);return 0;}
