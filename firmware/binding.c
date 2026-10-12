#include "binding.h"
#include <stddef.h>
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
int mc_binding_resolve(mc_binding *out,uint8_t *root,void *pattern_owner,mc_data_getter get) {
    mc_binding b={0};
    if(!out||!root||!pattern_owner||!get)return 0;
    if((uintptr_t)root>UINTPTR_MAX-620u||
       (uintptr_t)pattern_owner>UINTPTR_MAX-732u)return 0;
    uint8_t *p=pattern_owner;
    b.pattern_owner=p;b.global_owner=p+44;b.lock_owner=p+640;
    b.pattern=get(p);
    if(!b.pattern||(uintptr_t)b.pattern>UINTPTR_MAX-MC_UI_PATTERN_BYTES)return 0;
    b.global=get(b.global_owner);b.locks=get(b.lock_owner);
    if(b.global!=b.pattern+30642||b.locks!=b.pattern+4332)return 0;
    for(unsigned i=0;i<6;i++) {
        uint8_t *owner=p+112+88*i;
        b.track_owner[i]=owner;b.track[i]=get(owner);
        b.sound_owner[i]=root+212+68*i;b.sound[i]=get(b.sound_owner[i]);
        if(b.track[i]!=b.pattern+MC_UI_TRACK_BYTES*i||!b.sound[i]||
           (uintptr_t)b.sound[i]>UINTPTR_MAX-100u)return 0;
        /* Track's lock owner/index and global pattern parent must agree with
         * the object tree; these are target big-endian 32-bit fields. */
        if(be32(owner+44)!=(uint32_t)(uintptr_t)b.lock_owner||
           be32(owner+56)!=i||be32(owner+60)!=(uint32_t)(uintptr_t)p)return 0;
    }
    *out=b;
    return 1;
}
