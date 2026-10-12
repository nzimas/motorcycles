#include "interface.h"
static int before(uint32_t a, uint32_t b) { return (int32_t)(a-b)<0; }
void mc_init(mc_interface *s) {
    unsigned char *bytes=(unsigned char *)s;
    for (unsigned i=0; i<sizeof(*s); i++) bytes[i]=0;
    s->music=0x99;
    for (unsigned g=0; g<4; g++) s->level[g]=MC_LEVEL_DEFAULT;
}
int mc_event_handle(mc_interface *s, unsigned ev, unsigned i, int v,
                    unsigned target, uint32_t now, mc_publish publish, void *ctx) {
    if (ev == MC_PAGE) {
        if (v && !s->page) {
            s->page=1; s->page_exit=(uint8_t)(s->state!=MC_INACTIVE);
        }
        if (!v) {
            int active=s->state!=MC_INACTIVE;
            s->page=0;
            if(s->page_exit) {
                s->state=MC_INACTIVE;s->held=0;s->armed=0;s->fired=0;
                s->feedback=0;s->acknowledgement=0;s->generated=0;s->audition=0;
            }
            s->page_exit=0;
            return active;
        }
        return s->state != MC_INACTIVE;
    }
    if(ev==MC_STEP&&i<16&&!v&&(s->scale_down&(1u<<i))) {
        s->scale_down&=(uint16_t)~(1u<<i);return 1;
    }
    /* Releases of keys pressed in generator mode stay consumed, even after PAGE
     * or inside the scale menu. */
    if (ev==MC_STEP && i<15 && !v && (s->level_down & (1u<<i))) {
        s->level_down &= (uint16_t)~(1u<<i);
        return 1;
    }
    if (ev==MC_STEP && i==15 && !v && (s->down & 1u)) {
        s->down = 0;
        if (s->held) {
            if (s->state==MC_HOLD && !s->fired && s->generated && s->generator==0
                && (uint32_t)(now-s->since) < MC_AUDITION_TAP_MS) s->audition=1;
            s->held=0;
            if (s->state!=MC_INACTIVE) s->state=MC_READY;
        }
        return 1;
    }
    if(ev==MC_PAD&&i==4&&v&&s->page) {
        if(s->pad_down&(1u<<i))return 1;
        s->pad_down|=(uint8_t)(1u<<i);
        s->page_exit=0;s->state=MC_SCALE;s->held=0;s->armed=0;
        s->fired=0;s->generated=0;s->audition=0;s->feedback=0;
        return 1;
    }
    if(s->state==MC_SCALE) {
        if(ev==MC_STEP) {
            if(i>=16||!v)return 1;
            uint16_t bit=(uint16_t)(1u<<i);
            if(s->scale_down&bit)return 1;
            s->scale_down|=bit;
            uint8_t music=(uint8_t)(128|(i<4?((i<<4)|(s->music&15)):
                ((s->music&0x30)|(i-4))));
            mc_ui_request request={4,0,s->target,0,0,music};
            s->state=MC_GENERATING;
            int ok=publish&&publish(ctx,&request);
            if(s->state==MC_INACTIVE)return 1;
            if(ok)s->music=music;
            s->state=MC_SCALE;s->acknowledgement=(uint8_t)(ok?1:2);
            s->acknowledgement_until=now+800;
            return 1;
        }
        if(ev==MC_DENSITY||ev==MC_BRUTALITY)return 1;
    }
    if (ev==MC_PAD && i<6 && !v && (s->pad_down & (1u<<i))) {
        s->pad_down &= (uint8_t)~(1u<<i);
        return 1;
    }
    if (!s->page && s->state==MC_INACTIVE) return 0;
    if (ev==MC_PAD && i<4 && v) {
        if (s->pad_down & (1u<<i)) return 1;
        s->pad_down |= (uint8_t)(1u<<i);
        if (s->state==MC_INACTIVE) {
            if (target>=6) return 1;
            s->target=(uint8_t)target;
        }
        s->generator=(uint8_t)i; s->state=MC_READY; s->path=1;   /* the unified rhythmic path */
        s->page_exit=0;
        s->generated=0;s->audition=0;
        s->held=0; s->armed=1; s->fired=0; s->feedback=0;
        return 1;
    }
    if (s->state==MC_INACTIVE) return 0;
    if (ev==MC_PAD) {
        if (i<6 && v) s->pad_down|=(uint8_t)(1u<<i);
        return 1;
    }
    if (ev==MC_STEP) {
        if (i>=16 || !v) return 1;
        if (i<15) {                               /* level 1..15 */
            uint16_t bit=(uint16_t)(1u<<i);
            if (s->level_down & bit) return 1;
            s->level_down |= bit;
            s->level[s->generator]=(uint8_t)(i+1);
            s->feedback=1; s->feedback_until=now+1000;
            return 1;
        }
        if (s->down & 1u) return 1;               /* step 16: generate key */
        s->down |= 1u;
        s->held=1; s->fired=0; s->since=now; s->state=MC_HOLD;
        return 1;
    }
    /* Knobs keep their ordinary functions in generator mode. */
    if (ev==MC_DENSITY || ev==MC_BRUTALITY) return 0;
    if (ev==MC_TICK && s->state==MC_HOLD && s->held && !s->fired
        && (uint32_t)(now-s->since)>=MC_GENERATE_HOLD_MS) {
        unsigned level=s->level[s->generator];
        uint8_t amount=mc_level_amount(level);
        mc_ui_request r={s->generator,1,s->target,amount,amount,0};
        s->fired=1; s->state=MC_GENERATING;
        int ok=publish && publish(ctx,&r);
        s->feedback=0;
        /* Notifications may dispatch a release before publication returns. */
        if(s->state==MC_INACTIVE) {
            s->state=MC_INACTIVE;s->acknowledgement=0;return 0;
        }
        s->generated=(uint8_t)(ok&&s->generator==0);
        s->acknowledgement=(uint8_t)(ok?1:2);
        s->acknowledgement_until=now+800; s->state=MC_READY;
    }
    return ev!=MC_TICK;
}
uint16_t mc_leds(const mc_interface *s, uint32_t now) {
    if (s->state==MC_INACTIVE) return 0;
    if(s->state==MC_SCALE) {
        uint16_t leds=0xffff;
        if((now/400)%2)leds&=(uint16_t)~((1u<<((s->music>>4)&3))|
            (1u<<(4+(s->music&15))));
        return leds;
    }
    /* Success: a sweep from step 1 to 16 (400 ms), then two flashes of all
       sixteen. Failure: alternating steps blink. */
    if (s->acknowledgement && before(now,s->acknowledgement_until)) {
        uint32_t t=800u-(s->acknowledgement_until-now);
        if (s->acknowledgement==1) {
            if (t<400u) { unsigned n=1u+t*16u/400u; return (uint16_t)((1u<<n)-1u); }
            return (uint16_t)(((t-400u)/100u)%2u ? 0u : 0xffffu);
        }
        return (uint16_t)((now/200)%2 ? 0xaaaau : 0x5555u);
    }
    /* The level as a bar from step 1; step 16 blinks (fast while held). */
    unsigned level=s->level[s->generator];
    if (level>15) level=15;
    uint16_t leds=(uint16_t)((1u<<level)-1u);
    unsigned period=s->state==MC_HOLD&&s->held?100u:400u;
    if ((now/period)%2==0) leds|=0x8000u;
    return leds;
}
