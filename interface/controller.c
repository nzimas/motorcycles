#include "controller.h"
static int publish(void *context,const mc_ui_request *u) {
    mc_controller *c=context;
    if(u->generator==4){int ok=c->retune&&c->retune(c->context,c->music,u->music);if(ok)c->music=u->music;return ok;}
    mc_request r={0};
    r.music=c->music;r.note_valid_mask=c->note_valid_mask;
    for(unsigned i=0;i<MC_TRACKS;i++)r.default_note[i]=c->default_note[i];
    r.generator=u->generator;r.path=u->path;r.target=u->target;
    r.density=u->density;r.brutality=u->brutality;
    r.phrase_length=c->phrase_length;
    if(u->generator==MC_PHRASE){unsigned n=0;for(unsigned i=0;i<MC_TRACKS;i++)if(i!=u->target)n+=c->lock_pairs[i];r.locks_elsewhere=(uint8_t)(n>255?255:n);}r.source_sound_valid_mask=c->source_sound_valid_mask;
    r.machine_mask=c->machine_mask;r.machine_valid_mask=c->machine_valid_mask;
    for(unsigned i=0;i<MC_TRACKS;i++){r.current_machine[i]=c->current_machine[i];r.current_sound[i]=c->current_sound[i];}
    /* Advance each attempt, independent of slot/tempo/knob positions. */
    c->seed+=0x9e3779b9u;
    if(!c->seed)c->seed=0x6d2b79f5u;
    r.seed=c->seed^c->entropy;
    if(!r.seed)r.seed=0x6d2b79f5u;
    if(!c->commit || !mc_generate(&r,&c->scratch))return 0;
    return c->commit(c->context,&c->scratch,&r);
}
void mc_controller_init(mc_controller *c,uint32_t seed,uint8_t machines,
                        mc_commit_plan commit,void *context) {
    unsigned char *p=(unsigned char *)c;
    for(unsigned i=0;i<sizeof(*c);i++)p[i]=0;
    mc_init(&c->ui);c->music=0x99;c->seed=seed;c->machine_mask=machines;
    c->commit=commit;c->context=context;
}
int mc_controller_event(mc_controller *c,unsigned ev,unsigned i,int v,
                         unsigned target,uint32_t now) {
    c->entropy=(c->entropy^now^(ev<<24)^(i<<16)^(uint32_t)v)*0x01000193u;
    return mc_event_handle(&c->ui,ev,i,v,target,now,publish,c);
}
