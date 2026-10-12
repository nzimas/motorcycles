#include "../interface/controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static mc_plan content;
static unsigned calls;
static int refuse;static unsigned expect_level,expect_generator;
static int commit(void *ctx,const mc_plan *p,const mc_request *r) {
    (void)ctx;assert(r->target==4&&r->path==MC_RHYTHMIC&&r->generator==expect_generator);
    assert(r->density==mc_level_amount(expect_level)&&r->brutality==r->density);calls++;
    if(refuse)return 0;
    for(unsigned t=0;t<6;t++) {
        if(p->sound_mask&(1u<<t))content.sound[t]=p->sound[t];
        if(p->sequence_mask&(1u<<t))content.phrase[t]=p->phrase[t];
    }
    if(p->tempo)content.tempo=p->tempo;
    return 1;
}
static void event(mc_controller *c,unsigned e,unsigned i,int v,uint32_t ms) {
    mc_controller_event(c,e,i,v,4,ms);
}
int main(void) {
    mc_controller c;mc_controller_init(&c,123,63,commit,0);
    memset(&content,0xa5,sizeof content);
    static const unsigned levels[4]={1,7,11,15};
    for(unsigned g=0;g<4;g++)for(unsigned l=0;l<4;l++) {
        unsigned level=levels[l];expect_generator=g;expect_level=level;
        event(&c,MC_PAGE,0,1,0);event(&c,MC_PAD,g,1,1);event(&c,MC_PAD,g,0,2);
        event(&c,MC_STEP,level-1,1,3);event(&c,MC_STEP,level-1,0,4);
        assert(c.ui.level[g]==level&&c.ui.state==MC_READY);
        mc_plan prior=content;
        event(&c,MC_STEP,15,1,5);event(&c,MC_TICK,0,0,1504);
        assert(!memcmp(&content,&prior,sizeof content));
        event(&c,MC_TICK,0,0,1505);assert(c.ui.acknowledgement==1);
        assert(memcmp(&content,&prior,sizeof content));
        for(unsigned t=0;t<6;t++) {
            if(g==MC_PATCH&&t!=4)assert(!memcmp(&prior.sound[t],&content.sound[t],sizeof(mc_sound)));
            if(g==MC_PATCH||g==MC_KIT)assert(!memcmp(&prior.phrase[t],&content.phrase[t],sizeof(mc_phrase)));
            if(g==MC_PHRASE&&t!=4)assert(!memcmp(&prior.phrase[t],&content.phrase[t],sizeof(mc_phrase)));
            if(g>=MC_PHRASE)assert(!memcmp(&prior.sound[t],&content.sound[t],sizeof(mc_sound)));
        }
        unsigned n=calls;event(&c,MC_TICK,0,0,3000);assert(calls==n);
        prior=content;event(&c,MC_STEP,15,0,3001);event(&c,MC_STEP,15,1,3002);event(&c,MC_TICK,0,0,4502);
        assert(memcmp(&content,&prior,sizeof content));
        prior=content;refuse=1;event(&c,MC_STEP,15,0,4503);event(&c,MC_STEP,15,1,4504);event(&c,MC_TICK,0,0,6004);
        assert(!memcmp(&content,&prior,sizeof content)&&c.ui.acknowledgement==2);refuse=0;
        event(&c,MC_PAGE,0,0,6005);event(&c,MC_STEP,15,0,6006);
    }
    printf("PASS integrated interface→generator→semantic commit: 4 generators x 4 levels on path 1, level amounts, scope, fresh holds and refusal preservation\n");
}
