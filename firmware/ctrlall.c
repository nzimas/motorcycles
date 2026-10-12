/* Latched Control All.
 *
 * FUNC + TRK toggles the mode (it replaces the stock FUNC + TRK Track Setup
 * shortcut). While it is on:
 * - every knob acts as stock Control All (normally TRK held + turn) on all six
 *   tracks: the stock encoder handler's held-key query (0x4007faf4, reached
 *   through a register loaded at 0x4001f55c and 0x4001aaa8) answers "TRK held"
 *   through mc_held(), except while a trig is held for p-lock editing;
 * - FUNC + PITCH / FUNC + SWEEP move the DJ filter / distortion of all six
 *   tracks (firmware/drive_ui.c);
 * - parameters turned in the mode override step parameter locks: the stock
 *   knob-edit message handler (0x40059534: value, track, k) records each
 *   value, and the lock applier (0x400583da, called at 0x40058f36 for
 *   lock-only trigs and 0x40059194 for note trigs, in the audio dispatcher)
 *   is wrapped so every trig carries the Control All value for those k,
 *   including trigs that play a sound lock;
 * - the TRK LED stays lit and the screen blinks a large CTRL ALL.
 * Turning it off restores the six sounds exactly as they were when it was
 * turned on (or when the pattern last changed), and step locks apply again
 * from the next trig. Entering generator mode turns it off first, so a
 * generator never sees, or is undone by, provisional values.
 */
#include <stdint.h>
#include "../adapter/firmware.h"

#define TRK_KEY 2
#define FUNC_KEY 1
#define TRK_LED 31
#define LOCK_BUFFER_ENTRIES 66      /* stock pool block: 20 + 8 x 66 bytes */

extern uint16_t mc_irq_save(void);
extern void mc_irq_restore(uint16_t);
extern volatile uint32_t mc_music_context_stamp;
extern void mc_drive_overlay(char kind,int value,int ticks);
extern char mc_drive_overlay_kind(void);
extern void mc_drive_overlay_close(void);
extern void mc_drive_sync(void);

volatile uint8_t mc_ca_on;
/* Audio dispatcher side (one interrupt level, never pre-empts itself). */
static volatile uint32_t touched;            /* bit k: turned in this session */
static uint32_t have[6];                     /* bit k: value recorded for track */
static uint16_t value[6][32];
/* UI side. */
static uint8_t snapshot[6][100];
static uint8_t *snapshot_data[6];
static uint32_t snapshot_stamp;
static uint8_t swallow_trk;
static uint32_t since,next_show,quiet_until;
static uint8_t led_off_passes;

static int stock_held(unsigned code){return ((int (*)(unsigned))0x4007faf4u)(code);}
static int trig_held(void){
    void *ui=((void *(*)(void))0x400cf9a8u)();
    return ui&&(((int (*)(void *))0x4006bdfeu)(ui)&255);
}
/* Held-key query of the stock encoder handlers (codes 1, 2, 8, 12). */
int mc_held(unsigned code) {
    if(code==TRK_KEY&&mc_ca_on&&!trig_held())return 1;
    return stock_held(code);
}

/* Knob edit reaching the audio engine (dispatcher interrupt). */
void mc_ca_param(uint32_t v,uint32_t track,uint32_t k) {
    if(!mc_ca_on||track>=6||k>=32)return;
    value[track][k]=(uint16_t)v;have[track]|=1u<<k;touched|=1u<<k;
}
/* Step locks of one trig, just before the stock applier (dispatcher interrupt). */
void mc_ca_locks(int32_t track,uint8_t *buffer,void *cache) {
    uint32_t want=touched;
    if(mc_ca_on&&want&&(uint32_t)track<6&&buffer) {
        int32_t n=*(int32_t *)(buffer+8);uint32_t seen=0;
        const uint8_t *kit=*(uint8_t *const *)0x800017e4u;
        const uint8_t *sound=kit?kit+28+100*track:0;
        if(n<0)n=0;
        if(n>LOCK_BUFFER_ENTRIES)n=LOCK_BUFFER_ENTRIES;
        for(int32_t i=0;i<n;i++) {
            uint16_t *e=(uint16_t *)(buffer+20+8*i);unsigned k=e[0];
            if(k<32&&(want&(1u<<k))) {
                if(have[track]&(1u<<k))e[1]=value[track][k];
                else if(sound)e[1]=*(const uint16_t *)(sound+20+2*k);
                seen|=1u<<k;
            }
        }
        /* Trigs without a lock on k (in particular sound-locked trigs, whose
         * pool sound would otherwise bring its own value) get one. */
        uint32_t add=want&~seen;
        for(unsigned k=0;add&&k<32&&n<LOCK_BUFFER_ENTRIES;k++) {
            if(!(add&(1u<<k)))continue;
            add&=~(1u<<k);
            if(!(have[track]&(1u<<k))&&!sound)continue;
            uint16_t *e=(uint16_t *)(buffer+20+8*n);
            e[0]=(uint16_t)k;e[1]=(have[track]&(1u<<k))?value[track][k]:*(const uint16_t *)(sound+20+2*k);e[2]=0;e[3]=0;
            n++;
        }
        *(int32_t *)(buffer+8)=n;
    }
    ((void (*)(int32_t,uint8_t *,void *))0x400583dau)(track,buffer,cache);
}

static uint8_t *sound_data(unsigned t) {
    uint8_t *root=((void *(*)(void))0x400cf866u)();
    return root?mc_fw_owner_data(root+212+68*t):0;
}
static void take_snapshot(void) {
    for(unsigned t=0;t<6;t++) {
        uint8_t *d=sound_data(t);snapshot_data[t]=d;
        if(d)for(unsigned i=0;i<100;i++)snapshot[t][i]=d[i];
    }
    snapshot_stamp=mc_music_context_stamp;
}
static void forget_values(void) {
    uint16_t sr=mc_irq_save();
    touched=0;for(unsigned t=0;t<6;t++)have[t]=0;
    mc_irq_restore(sr);
}
static void enter(uint32_t now) {
    take_snapshot();
    forget_values();
    mc_ca_on=1;
    since=now;next_show=now;quiet_until=0;led_off_passes=0;
}
void mc_ca_exit(void) {
    if(!mc_ca_on)return;
    uint16_t sr=mc_irq_save();
    mc_ca_on=0;touched=0;for(unsigned t=0;t<6;t++)have[t]=0;
    mc_irq_restore(sr);
    /* Restore the sounds the mode started from, unless another pattern has
     * been loaded meanwhile (its sounds are already its saved ones). */
    uint8_t *root=((void *(*)(void))0x400cf866u)();
    if(root&&snapshot_stamp==mc_music_context_stamp)for(unsigned t=0;t<6;t++) {
        void *owner=root+212+68*t;
        uint8_t *d=mc_fw_owner_data(owner);
        if(!d||d!=snapshot_data[t])continue;
        unsigned differ=0;
        for(unsigned i=0;i<100;i++)differ|=d[i]^snapshot[t][i];
        if(!differ)continue;
        sr=mc_irq_save();
        for(unsigned i=0;i<100;i++)d[i]=snapshot[t][i];
        mc_irq_restore(sr);
        mc_fw_owner_notify(owner,0);
    }
    mc_drive_sync();
    if(mc_drive_overlay_kind()=='C')mc_drive_overlay_close();
    led_off_passes=20;
}

/* Key hook (FUNC + TRK). Returns 1 when consumed. */
int mc_ca_key(unsigned code,unsigned flags,uint32_t now) {
    if(code!=TRK_KEY)return 0;
    if((flags&1)&&!(flags&8)&&stock_held(FUNC_KEY)) {
        if(mc_ca_on)mc_ca_exit();else enter(now);
        swallow_trk=1;
        return 1;
    }
    if(swallow_trk) {
        if(flags&16)swallow_trk=0;
        return 1;
    }
    return 0;
}
/* Any knob turn while on: keep the blinking banner out of the way. */
void mc_ca_knob(uint32_t now) {
    if(!mc_ca_on)return;
    quiet_until=now+1500u;
    if(mc_drive_overlay_kind()=='C')mc_drive_overlay_close();
}
/* UI clock, inside an LED pass. */
void mc_ca_tick(uint32_t now) {
    if(!mc_ca_on&&!led_off_passes)return;
    void *leds=((void *(*)(void))0x400cfaf4u)();
    if(!mc_ca_on) {
        led_off_passes--;
        if(leds)((void (*)(void *,unsigned,unsigned,unsigned))0x40005f86u)(leds,TRK_LED,1,0);
        return;
    }
    if(leds)((void (*)(void *,unsigned,unsigned,unsigned))0x40005f86u)(leds,TRK_LED,2,0);
    /* Another pattern loaded: its sounds are the new starting point. */
    if(snapshot_stamp!=mc_music_context_stamp){take_snapshot();forget_values();}
    /* Banner: 500 ms on, 500 ms off. */
    if((int32_t)(now-quiet_until)<0)return;
    if((int32_t)(now-next_show)>=0) {
        if(mc_drive_overlay_kind()==0||mc_drive_overlay_kind()=='C')mc_drive_overlay('C',0,30);
        next_show=now+1000u;
    }
}
