#include "preparation.h"
#include "duration.h"
#include "../generation/sequence_encoding.h"
#include <stddef.h>
extern void *memcpy(void *,const void *,size_t);
static unsigned word(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static void put(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
int mc_prepare(mc_binding *b,const mc_request *q,const mc_plan *plan,mc_snapshots *s,
               const uint32_t durations[128],mc_stage_machine stage,
               struct mc_fw_transaction *out) {
    struct mc_fw_transaction t={0};
    if(!b||!q||!plan||!s||!out||q->generator>MC_PATTERN||q->target>=6)return 0;
    unsigned sm=q->generator==MC_PATCH?1u<<q->target:q->generator==MC_KIT?63:0;
    unsigned qm=q->generator==MC_PHRASE?1u<<q->target:q->generator==MC_PATTERN?63:0;
    if(plan->sound_mask!=sm||plan->sequence_mask!=qm||plan->volume_mask!=(q->generator==MC_PATTERN?63u:0u))return 0;
    for(unsigned i=0;i<6;i++)if(sm&(1u<<i)) {
        if(!stage||!b->sound[i]||!b->sound_owner[i])return 0;
        unsigned alignment=(unsigned)((uintptr_t)b->sound[i]&3u);
        if(alignment&1u)return 0;
        uint8_t *before=s->sound_before[i]+alignment,*after=s->sound_after[i]+alignment;
        memcpy(before,b->sound[i],100);memcpy(after,before,100);
        if(!stage(b->sound_owner[i],after,plan->sound[i].machine)||
           !mc_encode_engine(&plan->sound[i],after+20,80))return 0;
        /* LFO k1..8 and Volume+Dist, sends, pan k19..22. */
        if(!mc_encode_extras(&plan->extra[i],after+20,80))return 0;
        t.region[t.region_count++]=(struct mc_fw_region){b->sound[i],before,after,100};
        t.owner[t.owner_count++]=(struct mc_fw_owner){b->sound_owner[i],mc_fw_owner_notify};
    }
    /* Pattern: every track's VOLUME+DIST (sound word k19) to 127, nothing else
     * in the sound changes; same atomic publication and owner notification. */
    for(unsigned i=0;i<6;i++)if((plan->volume_mask&(1u<<i))&&!(sm&(1u<<i))) {
        if(!b->sound[i]||!b->sound_owner[i])return 0;
        unsigned alignment=(unsigned)((uintptr_t)b->sound[i]&3u);
        if(alignment&1u)return 0;
        uint8_t *before=s->sound_before[i]+alignment,*after=s->sound_after[i]+alignment;
        memcpy(before,b->sound[i],100);memcpy(after,before,100);
        after[20+2*19]=127;after[20+2*19+1]=0;
        t.region[t.region_count++]=(struct mc_fw_region){b->sound[i],before,after,100};
        t.owner[t.owner_count++]=(struct mc_fw_owner){b->sound_owner[i],mc_fw_owner_notify};
    }
    if(qm) {
        if(!b->pattern||!b->global||!b->locks||!durations)return 0;
        unsigned alignment=(unsigned)((uintptr_t)b->pattern&3u);
        if(alignment&1u)return 0;
        uint8_t *before=s->pattern_before+alignment,*after=s->pattern_after+alignment;
        memcpy(before,b->pattern,MC_UI_PATTERN_BYTES);memcpy(after,before,MC_UI_PATTERN_BYTES);
        unsigned scale=before[30667],global_length=word(before+30662),global_speed=before[30668],length=0;
        if(scale>1||global_speed>6||!global_length||global_length>1024)return 0;
        for(unsigned i=0;i<6;i++)if(qm&(1u<<i)) {
            uint8_t codes[13];unsigned offset=MC_UI_TRACK_BYTES*i;
            unsigned speed=q->generator==MC_PATTERN?2:scale?before[offset+715]:global_speed;
            unsigned effective=scale?word(before+offset+713):global_length;
            if(q->generator==MC_PHRASE&&(!effective||effective>64||
               !q->phrase_length||q->phrase_length!=effective||plan->phrase[i].length!=effective))return 0;
            if(!mc_duration_codes(speed,durations,codes)||
               !mc_stage_sequence(&plan->phrase[i],before+offset,after+offset,
                 after+4332+MC_UI_LOCK_BYTES*i,codes,1))return 0;
            if(plan->phrase[i].length>length)length=plan->phrase[i].length;
            t.owner[t.owner_count++]=(struct mc_fw_owner){b->track_owner[i],mc_fw_owner_notify};
        }
        /* The stock saver keeps at most 79 locked (track, slot) pairs and drops
         * the rest silently. Never publish a sequence that pushes a pattern over
         * that limit; a pattern the user already locked past it may only shrink. */
        {unsigned pairs=0,prior=0;for(unsigned i=0;i<6;i++){pairs+=mc_lock_pairs(after+4332+MC_UI_LOCK_BYTES*i);prior+=mc_lock_pairs(before+4332+MC_UI_LOCK_BYTES*i);}
         if(pairs>MC_LOCK_PAIRS&&pairs>prior)return 0;}
        if(q->generator==MC_PATTERN) {
            if(!length||plan->tempo<30||plan->tempo>300)return 0;
            put(after+30660,(unsigned)plan->tempo*120u);
            put(after+30662,length);
            after[30667]=0;after[30668]=2;
            t.region[t.region_count++]=(struct mc_fw_region){b->pattern,before,after,MC_UI_PATTERN_BYTES};
            t.owner[t.owner_count++]=(struct mc_fw_owner){b->global_owner,mc_fw_owner_notify};
        }else{
            unsigned offset=MC_UI_TRACK_BYTES*q->target,locks=4332+MC_UI_LOCK_BYTES*q->target;
            t.region[t.region_count++]=(struct mc_fw_region){b->pattern+offset,before+offset,after+offset,MC_UI_TRACK_BYTES};
            t.region[t.region_count++]=(struct mc_fw_region){b->pattern+locks,before+locks,after+locks,MC_UI_LOCK_BYTES};
        }
        t.owner[t.owner_count++]=(struct mc_fw_owner){b->lock_owner,mc_fw_owner_notify};
    }
    /* Preserve the verified platform hooks supplied by the binding. */
    t.stopped=out->stopped;t.context=out->context;
    t.irq_save=out->irq_save;t.irq_restore=out->irq_restore;
    *out=t;
    return 1;
}
