#include "runtime.h"
#include "binding.h"
#include "preparation.h"
#include "music_storage.h"
static mc_snapshots snapshots;
typedef struct { mc_binding binding;uint8_t *root; } live_context;
extern uint16_t mc_irq_save(void);
extern void mc_irq_restore(uint16_t);
static void *selected_pattern(void *root) {
    return ((void *(*)(void *))0x4000f208u)(root);
}
static uint32_t pointer_word(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int stopped(void *context) {
    live_context *c=context;
    if(((unsigned (*)(void))0x4005481au)())return 0;
    if(((void *(*)(void))0x400cf866u)()!=c->root||
       selected_pattern(c->root)!=c->binding.pattern_owner||
       mc_fw_owner_data(c->binding.pattern_owner)!=c->binding.pattern)return 0;
    if(mc_fw_owner_data(c->binding.global_owner)!=c->binding.global||
       mc_fw_owner_data(c->binding.lock_owner)!=c->binding.locks)return 0;
    for(unsigned i=0;i<6;i++) {
        const uint8_t *owner=c->binding.track_owner[i];
        if(mc_fw_owner_data(c->binding.sound_owner[i])!=c->binding.sound[i]||
           mc_fw_owner_data(c->binding.track_owner[i])!=c->binding.track[i]||
           pointer_word(owner+44)!=(uintptr_t)c->binding.lock_owner||
           pointer_word(owner+56)!=i||
           pointer_word(owner+60)!=(uintptr_t)c->binding.pattern_owner)return 0;
    }
    return 1;
}
int mc_commit_live(void *unused,const mc_plan *plan,const mc_request *request) {
    (void)unused;
    live_context context;
    struct mc_fw_transaction transaction={0};
    context.root=((void *(*)(void))0x400cf866u)();
    if(!context.root||!mc_binding_resolve(&context.binding,context.root,
       selected_pattern(context.root),mc_fw_owner_data)||!stopped(&context))return 0;
    transaction.context=&context;transaction.stopped=stopped;
    transaction.irq_save=mc_irq_save;transaction.irq_restore=mc_irq_restore;
    if(!mc_prepare(&context.binding,request,plan,&snapshots,
       (const uint32_t *)0x4010d814u,mc_fw_stage_machine,&transaction))return 0;
    /* A pattern whose scale was chosen (music != 0) keeps its explicit tag in
     * the same atomic publication. A pattern with no choice stays unset, so
     * the Rhythmic kick keeps defaulting to C until the user picks a root. */
    unsigned alignment=(unsigned)((uintptr_t)context.binding.pattern&3u);
    uint8_t *before=snapshots.pattern_before+alignment;
    uint8_t *after=snapshots.pattern_after+alignment;
    if(!request->music){}
    else if(request->generator<MC_PHRASE) {
        for(unsigned i=0;i<MC_MUSIC_BYTES;i++) {
            before[30642+MC_MUSIC_OFFSET+i]=context.binding.global[MC_MUSIC_OFFSET+i];
            after[30642+MC_MUSIC_OFFSET+i]=before[30642+MC_MUSIC_OFFSET+i];
        }
        if(!mc_music_write(after+30642,request->music))return 0;
        transaction.region[transaction.region_count++]=(struct mc_fw_region){
          context.binding.global+MC_MUSIC_OFFSET,before+30642+MC_MUSIC_OFFSET,
          after+30642+MC_MUSIC_OFFSET,MC_MUSIC_BYTES};
        transaction.owner[transaction.owner_count++]=(struct mc_fw_owner){context.binding.global_owner,mc_fw_owner_notify};
    }else{
        if(!mc_music_write(after+30642,request->music))return 0;
        if(request->generator==MC_PHRASE) {
            transaction.region[transaction.region_count++]=(struct mc_fw_region){
              context.binding.global+MC_MUSIC_OFFSET,before+30642+MC_MUSIC_OFFSET,
              after+30642+MC_MUSIC_OFFSET,MC_MUSIC_BYTES};
            transaction.owner[transaction.owner_count++]=(struct mc_fw_owner){context.binding.global_owner,mc_fw_owner_notify};
        }
    }
    return mc_fw_publish(&transaction);
}

extern void *memcpy(void *,const void *,unsigned long);
static int retune_effective(unsigned raw,int offset,unsigned old_root,
                            unsigned new_root,unsigned new_scale,uint8_t *out) {
    int effective=(int)raw+offset;
    if(raw>127)return 0;
    if(effective<0)effective=0;
    if(effective>127)effective=127;
    unsigned lower=offset>0?(unsigned)offset:0;
    unsigned upper=offset<0?(unsigned)(127+offset):127;
    uint8_t note;
    if(!mc_retune_note((unsigned)effective,old_root,new_root,new_scale,lower,upper,&note))return 0;
    *out=(uint8_t)((int)note-offset);return 1;
}
int mc_retune_live(void *unused,uint8_t old_music,uint8_t new_music) {
    (void)unused;
    live_context context;struct mc_fw_transaction transaction={0};
    uint8_t old_root,old_scale,new_root,new_scale;
    if(!mc_music_unpack(old_music,&old_root,&old_scale)||
       !mc_music_unpack(new_music,&new_root,&new_scale))return 0;
    (void)old_scale;
    context.root=((void *(*)(void))0x400cf866u)();
    if(!context.root||!mc_binding_resolve(&context.binding,context.root,
       selected_pattern(context.root),mc_fw_owner_data)||!stopped(&context)||
       mc_music_read(context.binding.global)!=(old_music?old_music:0x99))return 0;
    unsigned alignment=(unsigned)((uintptr_t)context.binding.pattern&3u);
    uint8_t *before=snapshots.pattern_before+alignment,*after=snapshots.pattern_after+alignment;
    memcpy(before,context.binding.pattern,MC_UI_PATTERN_BYTES);memcpy(after,before,MC_UI_PATTERN_BYTES);
    for(unsigned i=0;i<6;i++) {
        const uint8_t *sound=context.binding.sound[i];
        unsigned pitch=((unsigned)sound[40]<<8)|sound[41];
        if(pitch<40*256u||pitch>88*256u)return 0;
        int offset=(int)(pitch>>8)-64;
        unsigned track=722*i;uint8_t note;
        unsigned raw=before[track+712];
        if(!retune_effective(raw,offset,old_root,new_root,new_scale,&note))return 0;
        after[track+712]=note;
        for(unsigned step=0;step<64;step++) {
            unsigned flags=((unsigned)before[track+2*step]<<8)|before[track+2*step+1];
            if(!(flags&1))continue;
            /* Lock-only trigs play no note: leave them alone. */
            if((flags&0x0800)&&!(flags&0x0080))continue;
            unsigned stored=before[track+580+step],raw_step=stored==255?raw:stored;
            if(raw_step>127||before[track+644+step]!=255)return 0;
            /* Lock slot k is sound word k (stock UI lock path 4000bb94 and
             * playback 400583da): Pitch is slot 10. Preserve its value. */
            unsigned at=4332+4385*i+68*step+2*10;
            unsigned lock=((unsigned)before[at]<<8)|before[at+1];
            int step_offset=offset;
            if(lock!=65535) {
                if(lock<40*256u||lock>88*256u)return 0;
                step_offset=(int)(lock>>8)-64;
            }
            if(!retune_effective(raw_step,step_offset,old_root,new_root,new_scale,&note))return 0;
            if(stored!=255||note!=after[track+712])after[track+580+step]=note;
        }
        transaction.owner[transaction.owner_count++]=(struct mc_fw_owner){context.binding.track_owner[i],mc_fw_owner_notify};
    }
    if(!mc_music_write(after+30642,new_music))return 0;
    transaction.region[transaction.region_count++]=(struct mc_fw_region){context.binding.pattern,before,after,MC_UI_PATTERN_BYTES};
    transaction.owner[transaction.owner_count++]=(struct mc_fw_owner){context.binding.global_owner,mc_fw_owner_notify};
    transaction.stopped=stopped;transaction.context=&context;
    transaction.irq_save=mc_irq_save;transaction.irq_restore=mc_irq_restore;
    return mc_fw_publish(&transaction);
}
