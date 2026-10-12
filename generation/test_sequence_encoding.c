#include "sequence_encoding.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned i,j;mc_phrase p={0};uint8_t old[722],track[722],locks[4385],before[4385],codes[13]={0,14,30,38,46,50,54,58,62,64,66,68,70};
 for(i=0;i<722;i++)old[i]=(uint8_t)(i*7+3);
 memset(locks,0xa5,sizeof locks);p.length=32;p.step[7]=(mc_event){.on=1,.note=60,.velocity=99,.length=4};
 assert(mc_stage_sequence(&p,old,track,locks,codes,1));
 for(i=0;i<64;i++){unsigned flag=(i&1?16:0)|(i==7?0x881:0);assert(track[2*i]==flag>>8&&track[2*i+1]==(flag&255));assert(track[516+i]==255&&track[644+i]==255);for(j=0;j<66;j++)assert(locks[68*i+j]==255);assert(!locks[68*i+66]&&!locks[68*i+67]);}
 assert(track[135]==99&&track[199]==46&&track[587]==60);assert(track[713]==0&&track[714]==32);assert(!memcmp(track+512,old+512,4));assert(!memcmp(track+708,old+708,5));assert(!memcmp(track+715,old+715,4));assert(track[719]==100);assert(!memcmp(track+720,old+720,2));
 for(i=4352;i<4385;i++)assert(!locks[i]);
 memcpy(before,locks,sizeof before);memcpy(old,track,sizeof old);p.step[40].on=1;
 assert(!mc_stage_sequence(&p,old,track,locks,codes,1));assert(!memcmp(old,track,sizeof old)&&!memcmp(before,locks,sizeof before));
 p.step[40].on=0;p.step[7].length=0;assert(!mc_stage_sequence(&p,old,track,locks,codes,1));p.step[7].length=4;
 assert(mc_stage_sequence(&p,track,track,locks,codes,0));
 puts("PASS: canonical staging bounds, stale content replacement, preservation, alias, invalid atomicity");
}
