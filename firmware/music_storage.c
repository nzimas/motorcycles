#include "music_storage.h"
#include "../generation/music.h"
volatile uint32_t mc_music_context_stamp;
static int valid(const uint8_t *p) {
    uint8_t root,scale;
    return p&&p[0]=='M'&&p[1]=='C'&&p[2]=='G'&&p[3]=='4'&&p[4]==1&&
      (p[5]&128)&&mc_music_unpack(p[5],&root,&scale)&&
      p[6]==(uint8_t)(p[5]^0xa5)&&p[7]==(uint8_t)(p[5]+0x37);
}
int mc_music_owned(const uint8_t *global){return global&&valid(global+MC_MUSIC_OFFSET);}
uint8_t mc_music_read(const uint8_t *global){return mc_music_owned(global)?global[45]:0x99;}
int mc_music_write(uint8_t *global,uint8_t music){
    uint8_t root,scale;
    if(!global||!mc_music_unpack(music,&root,&scale))return 0;
    uint8_t explicit_music=(uint8_t)(128|(scale<<4)|root);
    uint8_t *p=global+MC_MUSIC_OFFSET;
    p[0]='M';p[1]='C';p[2]='G';p[3]='4';p[4]=1;p[5]=explicit_music;
    p[6]=(uint8_t)(explicit_music^0xa5);p[7]=(uint8_t)(explicit_music+0x37);
    return 1;
}
extern int mc_music_load_original(uint8_t *,const uint8_t *);
extern int mc_music_clear_original(uint8_t *,unsigned);
int mc_music_load_hook(uint8_t *dest,const uint8_t *src){
    int result=mc_music_load_original(dest,src);
    if((result&255)&&dest&&src){
        uint8_t *p=dest+MC_MUSIC_OFFSET;
        for(unsigned i=0;i<MC_MUSIC_BYTES;i++)p[i]=mc_music_owned(src)?src[MC_MUSIC_OFFSET+i]:0;
        mc_music_context_stamp++;
    }
    return result;
}
int mc_music_clear_hook(uint8_t *dest,unsigned keep){
    int result=mc_music_clear_original(dest,keep);
    if(dest&&!keep){for(unsigned i=0;i<MC_MUSIC_BYTES;i++)dest[MC_MUSIC_OFFSET+i]=0;mc_music_context_stamp++;}
    return result;
}
int mc_music_current_owned(void){
#ifdef __m68k__
    uintptr_t pattern=*(volatile uintptr_t *)0x40a7887cu;
    if((pattern&1)||pattern<0x40000000u||pattern>0x423380b0u-30710u)return 0;
    return mc_music_owned((const uint8_t *)pattern+30642);
#else
    return 0;
#endif
}
