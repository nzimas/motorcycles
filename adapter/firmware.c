#include "cave.h"
#include "firmware.h"
#include <stddef.h>
#ifdef __m68k__
extern int mc_fw_equal_blocks(const void *,const void *,uint32_t);
extern void mc_fw_copy_blocks(void *,const void *,uint32_t);
#endif
/* Bindings provide word-aligned firmware buffers. Unaligned prefixes/tails use
 * byte accesses; unsigned word loads are valid on the ColdFire target. */
static int equal(const uint8_t*a,const uint8_t*b,uint32_t n){
 while(n&&((uintptr_t)a&3)&&(((uintptr_t)a&3)==((uintptr_t)b&3))){
  if(*a++!=*b++)return 0;
  n--;
 }
 if((((uintptr_t)a|(uintptr_t)b)&3)==0){
#ifdef __m68k__
  uint32_t bulk=n&~31u;
  if(bulk){if(!mc_fw_equal_blocks(a,b,bulk))return 0;a+=bulk;b+=bulk;n-=bulk;}
#endif
  const uint32_t*x=(const uint32_t*)a,*y=(const uint32_t*)b;
  while(n>=16){if(x[0]!=y[0]||x[1]!=y[1]||x[2]!=y[2]||x[3]!=y[3])return 0;x+=4;y+=4;n-=16;}
  while(n>=4){if(*x++!=*y++)return 0;n-=4;}a=(const uint8_t*)x;b=(const uint8_t*)y;
 }
 if((((uintptr_t)a|(uintptr_t)b)&1)==0){
  const uint16_t*x=(const uint16_t*)a,*y=(const uint16_t*)b;
  while(n>=8){if(x[0]!=y[0]||x[1]!=y[1]||x[2]!=y[2]||x[3]!=y[3])return 0;x+=4;y+=4;n-=8;}
  while(n>=2){if(*x++!=*y++)return 0;n-=2;}a=(const uint8_t*)x;b=(const uint8_t*)y;
 }
 while(n--){if(*a++!=*b++)return 0;}
 return 1;
}
static void copy(uint8_t*a,const uint8_t*b,uint32_t n){
 while(n&&((uintptr_t)a&3)&&(((uintptr_t)a&3)==((uintptr_t)b&3))){
  *a++=*b++;n--;
 }
 if((((uintptr_t)a|(uintptr_t)b)&3)==0){
#ifdef __m68k__
  uint32_t bulk=n&~31u;
  if(bulk){mc_fw_copy_blocks(a,b,bulk);a+=bulk;b+=bulk;n-=bulk;}
#endif
  uint32_t*x=(uint32_t*)a;const uint32_t*y=(const uint32_t*)b;
  while(n>=16){x[0]=y[0];x[1]=y[1];x[2]=y[2];x[3]=y[3];x+=4;y+=4;n-=16;}
  while(n>=4){*x++=*y++;n-=4;}a=(uint8_t*)x;b=(const uint8_t*)y;
 }
 if((((uintptr_t)a|(uintptr_t)b)&1)==0){
  uint16_t*x=(uint16_t*)a;const uint16_t*y=(const uint16_t*)b;
  while(n>=8){x[0]=y[0];x[1]=y[1];x[2]=y[2];x[3]=y[3];x+=4;y+=4;n-=8;}
  while(n>=2){*x++=*y++;n-=2;}a=(uint8_t*)x;b=(const uint8_t*)y;
 }
 while(n--)*a++=*b++;
}
static int overlap(uintptr_t a,uint32_t an,uintptr_t b,uint32_t bn){return a<b+bn&&b<a+an;}
MC_CAVE_B int mc_fw_publish(struct mc_fw_transaction*t){
 uint32_t i,j,total=0;uint16_t sr;
 if(!t||!t->stopped||!t->irq_save||!t->irq_restore||!t->region_count||
 t->region_count>MC_FW_MAX_REGIONS||t->owner_count>MC_FW_MAX_REGIONS)return 0;
 for(i=0;i<t->region_count;i++){
  struct mc_fw_region*r=&t->region[i];
  if(!r->live||!r->before||!r->after||!r->size||r->size>30710||
  (uintptr_t)r->live+r->size<(uintptr_t)r->live||
  (uintptr_t)r->before+r->size<(uintptr_t)r->before||
  (uintptr_t)r->after+r->size<(uintptr_t)r->after)return 0;
  total+=r->size;if(total>64000)return 0;
 }
 for(i=0;i<t->region_count;i++){
  struct mc_fw_region*r=&t->region[i];
  for(j=0;j<t->region_count;j++){
   struct mc_fw_region*s=&t->region[j];
   if(j<i&&overlap((uintptr_t)r->live,r->size,(uintptr_t)s->live,s->size))return 0;
   /* Staging must never alias any live region being written. */
   if(s->size&&(overlap((uintptr_t)r->before,r->size,(uintptr_t)s->live,s->size)||
   overlap((uintptr_t)r->after,r->size,(uintptr_t)s->live,s->size)))return 0;
  }
 }
 for(i=0;i<t->owner_count;i++)if(!t->owner[i].object||!t->owner[i].notify)return 0;
 for(i=0;i<t->region_count;i++)if(!equal(t->region[i].live,t->region[i].before,t->region[i].size))return 0;
 if(!t->stopped(t->context))return 0;
 /* Final compare inside critical section prevents a partial commit following
  * transport or storage changes. No allocation/generation/observer calls here. */
 sr=t->irq_save();
 if(!t->stopped(t->context)){t->irq_restore(sr);return 0;}
 for(i=0;i<t->region_count;i++)if(!equal(t->region[i].live,t->region[i].before,t->region[i].size)){
  t->irq_restore(sr);return 0;
 }
 for(i=0;i<t->region_count;i++)copy(t->region[i].live,t->region[i].after,t->region[i].size);
 t->irq_restore(sr);
 for(i=0;i<t->owner_count;i++)t->owner[i].notify(t->owner[i].object,NULL);
 return 1;
}
void *mc_fw_owner_data(void*object){
 uintptr_t*v=*(uintptr_t**)object;
 return ((void*(*)(void*))v[10])(object);
}
void mc_fw_owner_notify(void*object,const void*event){
 uintptr_t*v=*(uintptr_t**)object;
 ((void(*)(void*,const void*))v[4])(object,event);
}
/* Highest machine index on the selected build: MACRO (6). */
#define MC_FW_LAST_MACHINE 6
int mc_fw_stage_machine(void *live_owner,uint8_t after[100],uint8_t machine){
#ifdef __m68k__
 uint32_t clone[17];uintptr_t*vt;uint8_t*live;uint8_t*obj=(uint8_t*)clone;
 if(!live_owner||!after||machine>
#ifdef MC_SELECTED_MODS
 MC_FW_LAST_MACHINE
#else
 5
#endif
 ||((uintptr_t)after&1)||
 (uintptr_t)after+100<(uintptr_t)after)return 0;
 vt=*(uintptr_t**)live_owner;
 if(!vt||vt[10]!=0x400d5632u)return 0;
 live=mc_fw_owner_data(live_owner);
 if(!live||overlap((uintptr_t)after,100,(uintptr_t)live,100)||
 overlap((uintptr_t)after,100,(uintptr_t)live_owner,68))return 0;
 if(!equal(after,live,100))return 0;
 if(after[38]==machine)return 1;
 copy(obj,(const uint8_t*)live_owner,68);
 *(uint32_t*)(obj+16)=(uint32_t)(uintptr_t)after;
 obj[12]=1;obj[36]=1;
 after[38]=machine;after[39]=0;
 /* Stock machine-defaults staging; standalone MACRO extends its tables. */
 return ((int(*)(void*))0x40014072u)(obj)&255;
#else
 (void)live_owner;(void)after;(void)machine;return 0;
#endif
}
