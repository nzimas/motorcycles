#include "music.h"
/* Major, natural minor, whole tone, Hirajoshi0,2,3,7,8. */
static const uint8_t intervals[4][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,4,6,8,10,0},{0,2,3,7,8,0,0}};
static const uint8_t counts[4]={7,7,6,5};
int mc_music_unpack(uint8_t packed,uint8_t *root,uint8_t *scale){
 if(!root||!scale)return 0;
 if(!packed){*root=9;*scale=MC_NATURAL_MINOR;return 1;}
 if(!(packed&128)||(packed&64)||(packed&15)>11)return 0;
 *root=packed&15;*scale=(packed>>4)&3;return 1;
}
int mc_music_pack(unsigned root,unsigned scale,uint8_t *packed){
 if(!packed||root>11||scale>3)return 0;
 *packed=(uint8_t)(128|(scale<<4)|root);return 1;
}
int mc_note_in_scale(unsigned note,unsigned root,unsigned scale){
 unsigned i,pitchclass;
 if(note>127||root>11||scale>3)return 0;
 pitchclass=(note+12-root)%12;
 for(i=0;i<counts[scale];i++)if(pitchclass==intervals[scale][i])return 1;
 return 0;
}
int mc_quantize_note(unsigned note,unsigned root,unsigned scale,unsigned lower,unsigned upper,uint8_t *out){
 unsigned d;
 if(!out||note>127||root>11||scale>3||lower>upper||upper>127)return 0;
 /* Search outward from the note; the lower candidate first resolves ties down. */
 for(d=0;d<=127;d++){
  if(note>=d&&note-d>=lower&&note-d<=upper&&mc_note_in_scale(note-d,root,scale)){*out=(uint8_t)(note-d);return 1;}
  if(note+d<=upper&&note+d>=lower&&mc_note_in_scale(note+d,root,scale)){*out=(uint8_t)(note+d);return 1;}
 }
 return 0;
}
int mc_retune_note(unsigned note,unsigned old_root,unsigned new_root,unsigned new_scale,unsigned lower,unsigned upper,uint8_t *out){
 int delta,value;
 if(note>127||old_root>11||new_root>11||new_scale>3)return 0;
 delta=(int)new_root-(int)old_root;if(delta>6)delta-=12;if(delta<-5)delta+=12;
 value=(int)note+delta;if(value<0)value=0;if(value>127)value=127;
 return mc_quantize_note((unsigned)value,new_root,new_scale,lower,upper,out);
}
