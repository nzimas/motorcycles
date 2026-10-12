#include "../firmware/music_storage.h"
#include "../generation/music.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int load_result=1;
int mc_music_load_original(uint8_t *d,const uint8_t *s){if(!d||!s)return 0;if(load_result){memmove(d,s,29);d[38]=s[38];}return load_result;}
int mc_music_clear_original(uint8_t *d,unsigned keep){if(d&&!keep){memset(d,0,29);d[38]=0;}return 0;}
int main(void){uint8_t bank[96][64],file[96][64],loaded[96][64],old[64];unsigned checks=0;
 memset(bank,0x5a,sizeof bank);
 for(unsigned p=0;p<96;p++){uint8_t packed;assert(mc_music_pack(p%12,(p/12)%4,&packed));memcpy(old,bank[p],64);assert(mc_music_write(bank[p],packed));assert(mc_music_read(bank[p])==packed);assert(!memcmp(bank[p],old,40)&&!memcmp(bank[p]+48,old+48,16));memcpy(file[p],bank[p],64);checks++;}
 memset(loaded,0xa5,sizeof loaded);
 for(unsigned p=0;p<96;p++){unsigned stamp=mc_music_context_stamp;assert(mc_music_load_hook(loaded[p],file[p])==1);assert(mc_music_context_stamp==stamp+1&&mc_music_read(loaded[p])==mc_music_read(bank[p]));assert(!memcmp(loaded[p],file[p],29));assert(loaded[p][39]==0xa5&&loaded[p][48]==0xa5);checks++;}
 for(unsigned byte=0;byte<8;byte++){memcpy(old,file[0],64);old[40+byte]^=1;assert(!mc_music_owned(old)&&mc_music_read(old)==0x99);assert(mc_music_load_hook(loaded[0],old)==1);assert(!memcmp(loaded[0]+40,(uint8_t[8]){0},8));checks++;}
 /* Existing copy semantics carry settings while source remains untouched. */
 assert(mc_music_load_hook(loaded[95],file[1])==1);assert(mc_music_read(loaded[95])==mc_music_read(file[1]));assert(mc_music_read(file[95])!=mc_music_read(file[1]));
 memcpy(old,loaded[95],64);unsigned stamp=mc_music_context_stamp;mc_music_clear_hook(loaded[95],1);assert(!memcmp(old,loaded[95],64)&&mc_music_context_stamp==stamp);mc_music_clear_hook(loaded[95],0);assert(mc_music_read(loaded[95])==0x99&&!mc_music_owned(loaded[95])&&mc_music_context_stamp==stamp+1);
 memcpy(old,loaded[1],64);load_result=0;stamp=mc_music_context_stamp;assert(!mc_music_load_hook(loaded[1],file[2])&&!memcmp(old,loaded[1],64)&&mc_music_context_stamp==stamp);
 for(unsigned b=1;b<256;b++)if(!(b&128)||(b&64)||(b&15)>11){assert(!mc_music_write(loaded[1],b)&&!memcmp(old,loaded[1],64));checks++;}
 assert(!mc_music_write(0,0)&&!mc_music_owned(0)&&mc_music_read(0)==0x99);
 printf("PASS %u independent96-pattern isolation/tag validation/copy/default/failure lifecycle checks\n",checks);return 0;}
