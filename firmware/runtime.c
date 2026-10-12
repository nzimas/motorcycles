#include "runtime.h"
#include "../adapter/firmware.h"
#include "binding.h"
#include "music_storage.h"
#include "../generation/sequence_encoding.h"
int mc_drive_encoder(const uint8_t *,unsigned);
void mc_drive_tick(uint32_t);
void mc_drive_sync(void);
int mc_ca_key(unsigned,unsigned,uint32_t);
void mc_ca_knob(uint32_t);
void mc_ca_tick(uint32_t);
void mc_ca_exit(void);
extern volatile uint8_t mc_ca_on;
static uint32_t clock_raw,clock_ms,clock_fraction;
static unsigned initialized,busy;
static void *pattern_seen;
static uint32_t pattern_stamp;
static unsigned audition_active,audition_track,audition_note;
static uint32_t audition_until;
extern uint8_t __bss_start[],_end[];
mc_controller mc_native;
uint32_t mc_led_previous;
extern void mc_show_popup(const char *);
static int commit_checked(void *context,const mc_plan *plan,const mc_request *request) {
    if(request->machine_valid_mask!=63)return 0;
    if(request->generator==MC_PHRASE&&!request->phrase_length)return 0;
    int result=mc_commit_live(context,plan,request);
    if(result) {
        static const char *const messages[4]={"PATCH CREATED","KIT CREATED",
            "PHRASE CREATED","PATTERN CREATED"};
        mc_show_popup(messages[request->generator]);
    }
    return result;
}
static uint32_t word(const uint8_t*p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
uint32_t mc_now(void) {
    uint32_t raw=*(volatile uint32_t *)0xfc07000cu;
    uint32_t elapsed=raw-clock_raw;
    clock_raw=raw;
    clock_ms+=elapsed/135168u;
    clock_fraction+=elapsed%135168u;
    clock_ms+=clock_fraction/135168u;
    clock_fraction%=135168u;
    return clock_ms;
}
void mc_native_init(void) {
    mc_controller_init(&mc_native,0x6379636cu,
#ifdef MC_SELECTED_MODS
        0x7f, /* stock 0..5 + MACRO 6 */
#else
        63,
#endif
        commit_checked,0);
    clock_raw=*(volatile uint32_t *)0xfc07000cu;
    clock_ms=clock_fraction=mc_led_previous=busy=0;
    audition_active=0;audition_until=0;audition_track=0;audition_note=0;
    mc_native.retune=mc_retune_live;
    pattern_seen=0;pattern_stamp=mc_music_context_stamp;
    initialized=1;
}
void mc_startup(void) {
    for(uint8_t *p=__bss_start;p<_end;p++)*p=0;
    mc_native_init();
}
static unsigned track(void) {
    void *root=((void *(*)(void))0x400cf866u)();
    void *kit=((void *(*)(void *))0x4000eb90u)(root);
    return ((unsigned (*)(void *))0x40012412u)(kit);
}
void mc_read_machines(void) {
    uint8_t *root=((void *(*)(void))0x400cf866u)();
    mc_native.machine_valid_mask=0;
    mc_native.source_sound_valid_mask=0;
    mc_native.note_valid_mask=0;
    if(!root)return;
    for(unsigned i=0;i<MC_TRACKS;i++) {
        const uint8_t *sound=mc_fw_owner_data(root+212+68*i);
        if(!sound)continue;
        /* Canonical 8.8 machine slot. Unsupported or fractional identities
         * must reach generator validation, never silently become stock Kick. */
        unsigned machine=((unsigned)sound[38]<<8)|sound[39];
        mc_native.current_machine[i]=(machine&255)?255:(uint8_t)(machine>>8);
        mc_native.machine_valid_mask|=(uint8_t)(1u<<i);
        uint8_t *semantic=(uint8_t *)&mc_native.current_sound[i];
        for(unsigned j=0;j<10;j++)semantic[j]=sound[38+2*j];
        if(!(machine&255)&&semantic[0]<MC_MACHINES&&semantic[1]>=40&&semantic[1]<=88&&
           semantic[2]<=127&&semantic[3]<=127&&semantic[4]<=127&&semantic[5]<=127&&
           semantic[6]<=1&&semantic[7]<=1&&semantic[8]<=127&&semantic[9]<=127&&
           (semantic[0]!=MC_MACRO||semantic[3]<=46))
            mc_native.source_sound_valid_mask|=(uint8_t)(1u<<i);
    }
    void *notes_pattern=((void *(*)(void *))0x4000f208u)(root);
    if(pattern_seen&&(pattern_seen!=notes_pattern||pattern_stamp!=mc_music_context_stamp)) {
        mc_native.ui.state=MC_INACTIVE;mc_native.ui.held=0;mc_native.ui.armed=0;
        mc_native.ui.generated=0;mc_native.ui.audition=0;mc_native.ui.page_exit=0;
    }
    pattern_seen=notes_pattern;pattern_stamp=mc_music_context_stamp;
    const uint8_t *pattern_data=notes_pattern?mc_fw_owner_data(notes_pattern):0;
    /* Generator sees 0 when no scale was chosen (A natural minor for notes, C
     * for the Rhythmic kick); the LEDs still show the A natural minor default. */
    mc_native.ui.music=mc_music_read(pattern_data?pattern_data+30642:0);
    mc_native.music=mc_music_owned(pattern_data?pattern_data+30642:0)?mc_native.ui.music:0;
    if(notes_pattern)for(unsigned i=0;i<MC_TRACKS;i++) {
        void *line=((void *(*)(void *,unsigned))0x4000cfccu)(notes_pattern,i);
        if(!line)continue;
        unsigned note=((unsigned (*)(void *))0x40015a34u)(line)&255u;
        if(note>127)continue;
        mc_native.default_note[i]=(uint8_t)note;
        mc_native.note_valid_mask|=(uint8_t)(1u<<i);
    }
    mc_native.phrase_length=0;
    if(mc_native.ui.generator==MC_PHRASE&&mc_native.ui.target<6) {
        void *pattern_owner=((void *(*)(void *))0x4000f208u)(root);
        mc_binding binding;
        if(mc_binding_resolve(&binding,root,pattern_owner,mc_fw_owner_data)) {
            unsigned length=0;
            if(binding.global[25]==0)
                length=((unsigned)binding.global[20]<<8)|binding.global[21];
            else if(binding.global[25]==1) {
                const uint8_t *line=binding.track[mc_native.ui.target];
                length=((unsigned)line[713]<<8)|line[714];
            }
            if(length&&length<=64)mc_native.phrase_length=(uint8_t)length;
            for(unsigned t=0;t<MC_TRACKS;t++)mc_native.lock_pairs[t]=(uint8_t)mc_lock_pairs(binding.locks+4385*t);
        }
    }
}
static void repaint(void) {
    void *controller=((void *(*)(void))0x400d0974u)();
    void *view=((void *(*)(void *))0x400060d8u)(controller);
    if(view)((void (*)(void *))0x40076c04u)(view);
}
/* Stock pad core ABI (4001d05e), also used by Model-TG slice audition.
 * Bypass the pad recording/MIDI wrapper; release via its matching note-off. */
static void audition_stop(void) {
    if(!audition_active)return;
    ((void (*)(unsigned,unsigned,unsigned))0x4008145eu)(audition_track,audition_note,0x40);
    audition_active=0;
}
static void audition_tick(uint32_t now) {
    if(audition_active&&(mc_native.ui.state==MC_INACTIVE||
       (int32_t)(now-audition_until)>=0))audition_stop();
}
static void audition_start(uint32_t now) {
    audition_stop();
    /* The ordinary pad core participates in live recording while running. */
    if(((unsigned (*)(void))0x4005481au)()!=0)return;
    audition_track=mc_native.ui.target;
    if(audition_track>=6)return;
    void *root=((void *(*)(void))0x400cf866u)();
    if(!root)return;
    void *pattern=((void *(*)(void *))0x4000f208u)(root);
    if(!pattern)return;
    void *line=((void *(*)(void *,unsigned))0x4000cfccu)(pattern,audition_track);
    if(!line)return;
    /* Same default-note getter used by normal pad dispatch4001d1fc. */
    audition_note=((unsigned (*)(void *))0x40015a34u)(line)&255u;
    if(audition_note>127)return;
    ((void (*)(unsigned,unsigned,unsigned,unsigned,unsigned,int,int))0x4008171eu)
        (audition_track,audition_note,100,0x40,0,-1,-1);
    audition_active=1;audition_until=now+150;
}
static int input(unsigned type,unsigned index,int value) {
    if(!initialized)mc_native_init();
    if(busy)return mc_native.ui.state!=MC_INACTIVE;
    busy=1;
    unsigned selected=mc_native.ui.state==MC_INACTIVE?track():mc_native.ui.target;
    uint32_t now=mc_now();
    audition_tick(now);
    mc_read_machines();
    unsigned was_inactive=mc_native.ui.state==MC_INACTIVE;
    int consumed=mc_controller_event(&mc_native,type,index,value,selected,now);
    /* Generators work on the saved sounds: leave Control All first. */
    if(was_inactive&&mc_native.ui.state!=MC_INACTIVE&&mc_ca_on)mc_ca_exit();
    /* Confirmation publication runs on ordinary input dispatch, never the LED
     * callback. Stock repeats arrive at 120Hz-derived intervals while held. */
    mc_controller_event(&mc_native,MC_TICK,0,0,selected,now);
    if(mc_native.ui.audition){mc_native.ui.audition=0;audition_start(now);}
    audition_tick(now);
    if(mc_native.ui.state!=MC_INACTIVE||consumed)repaint();
    busy=0;
    return consumed;
}
int mc_key(void *controller,const uint8_t *event) {
    (void)controller;
    unsigned code=word(event+12),flags=word(event+16);
    /* FUNC + TRK: latched Control All on/off (firmware/ctrlall.c). */
    if(initialized&&mc_ca_key(code,flags,mc_now()))return 1;
    if(code==15) {
        if(flags&16)return input(MC_PAGE,0,0);
        if(flags&1)return input(MC_PAGE,0,1);
        return initialized&&mc_native.ui.state!=MC_INACTIVE;
    }
    if(code>=16&&code<32) {
        if(flags&16)return input(MC_STEP,code-16,0);
        if(flags&1)return input(MC_STEP,code-16,1);
        if(initialized&&mc_native.ui.state!=MC_INACTIVE) {
            input(MC_TICK,0,0);return 1;
        }
    }
    return 0;
}
int mc_pad(void *controller,const uint8_t *event) {
    (void)controller;
    unsigned pad=word(event+20),down=word(event+16);
    if(!pad||pad>6||down>1)return 0;
    return input(MC_PAD,pad-1,(int)down);
}
int mc_encoder(void *controller,const uint8_t *event) {
    (void)controller;
    unsigned encoder=word(event+12);
    if(initialized)mc_ca_knob(mc_now());
    /* FUNC + SWEEP sets the selected track's distortion amount, in any mode. */
    /* FUNC + SWEEP: distortion amount; FUNC + PITCH: DJ filter (any mode). */
    if(initialized&&(encoder==6||encoder==2)&&mc_drive_encoder(event,track()))return 1;
    /* Generator levels are set on the step keys: knobs keep their ordinary functions. */
    return 0;
}
unsigned mc_led_mode(unsigned led,unsigned original) {
    if(!initialized||mc_native.ui.state==MC_INACTIVE)return original;
    const uint32_t *map=*(const uint32_t **)0x404a8bdcu;
    uint16_t mask=mc_leds(&mc_native.ui,clock_ms);
    if(map)for(unsigned i=0;i<16;i++)if(map[i]==led)return (mask&(1u<<i))?2:1;
    return original;
}
void mc_clock(void) {
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0":"=d"(sr));
    if(sr&0x0700)return;
    if(!initialized||busy)return;
    audition_tick(mc_now());
    mc_drive_tick(mc_now());
    mc_drive_sync();
    mc_ca_tick(mc_now());
    if(mc_native.ui.state==MC_INACTIVE)return;
    unsigned mask=mc_leds(&mc_native.ui,clock_ms);
    if(mask!=mc_led_previous) {mc_led_previous=mask;repaint();}
}
