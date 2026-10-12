/* Per-track distortion stage: audio processing, per-sound storage, and the
 * FUNC + SWEEP amount control with its on-screen "S" overlay.
 *
 * Each sound carries 8 distortion bytes at +76..+83 of the 100-byte runtime
 * record (sound words k28..k31, unused by the stock OS and by MACRO, which
 * reads only k10..k18). The stock loader zeroes them and the savers never
 * write them, so the save/load wrappers below carry them in the serialized
 * record at the same offsets with a signature at +88..+91. Only signed records
 * restore the settings; everything else (stock records, and Model-TG-era
 * records whose own data sits at +76..+87) loads with the stage off.
 */
#include <stdint.h>
#include "drive.h"
#include "djfilter.h"
#include "../adapter/firmware.h"

#define DRIVE_AT 76
#define SWEEP_ENCODER 6        /* panel knob 5: Sweep -> distortion amount */
#define PITCH_ENCODER 2        /* panel knob 1: Pitch -> DJ filter */
#define FILTER_BYTE 6          /* record byte 6: filter position (int8, 0 off) */
#define RESONANCE_BYTE 7       /* record byte 7: filter resonance (0 default) */
#define NOTIFY_AFTER_MS 300u

extern uint16_t mc_irq_save(void);
extern void mc_irq_restore(uint16_t);
extern void mc_show_popup(const char *);
extern void *mc_popup_last;
extern uint32_t mc_now(void);

/* ---- Audio: after stock Volume+Dist, before pan/mixer/sends/master. ------- */
static mc_drive_state states[6];
static mc_djfilter_state filters[6];
/* CPU guard. Stage cost in instructions per sample, measured in the packaged
 * firmware: 21 for the clipper, +9 tone, +9 bias, +18 low crossover, or +24
 * fold (which engages the crossover itself); drive alone is free; bypass ~0.
 * Whatever is stored, the six tracks together never exceed 6 x 30 per sample:
 * the costliest tracks lose fold, then bias, then low, then tone/drive for this
 * block. An overloaded audio interrupt starves the 6-channel USB stream first. */
/* Joint budget for distortion + DJ filter across six tracks, in instructions
 * per sample: 240 (~7.7k instructions per block, ~7% of the CPU). A running
 * DJ filter costs ~37 and is never trimmed; only distortion features are. */
#define DRIVE_BUDGET 240u
#define FILTER_COST 37u
static unsigned stage_cost(const uint8_t *d) {
    if(!d[MC_DRIVE_AMOUNT])return 0;
    return 21u+(d[MC_DRIVE_TONE]?9u:0u)+(d[MC_DRIVE_BIAS]?9u:0u)+(d[MC_DRIVE_FOLD]?24u:d[MC_DRIVE_LOW]?18u:0u);
}
/* Noise gate. The stage adds a lot of gain, so between hits it would lift the
 * noise floor and quiet tails into constant static. A peak follower on the
 * stage's input (one value per 32-sample block) opens the gate instantly above
 * -60 dBFS, so the first transient is fully distorted. Below -66 dBFS
 * (hysteresis) it holds for ~20 ms, then fades the distorted signal back to
 * the clean input over ~40 ms. While closed the stage does not run at all
 * (the clean signal, far below audibility, passes untouched) and costs only
 * the level check. */
#define GATE_OPEN  2147484L     /* -60 dBFS of Q31 full scale */
#define GATE_CLOSE 1073742L     /* -66 dBFS */
#define GATE_HOLD  30u          /* blocks of 0.667 ms: 20 ms */
#define GATE_STEP  546          /* 32768/60: 40 ms release */
typedef struct { int32_t env; int32_t gain; uint32_t hold; } gate_state;
static gate_state gates[6];
static void gated_block(int32_t *x,const uint8_t *params,unsigned t) {
    gate_state *g=&gates[t];
    if(!params[MC_DRIVE_AMOUNT]) {        /* stage off: bypass, gate resets */
        g->env=0;g->gain=0;g->hold=0;
        mc_drive_block(x,params,&states[t]);
        return;
    }
    int32_t peak=0;
    /* Every 4th sample is plenty for a gate; v^(v>>31) is |v| without overflow. */
    for(unsigned i=0;i<32;i+=4){int32_t v=x[i];v^=v>>31;if(v>peak)peak=v;}
    g->env=peak>(g->env>>1)?peak:(g->env>>1);   /* about -6 dB per block between peaks */
    int32_t from=g->gain;
    if(g->env>=GATE_OPEN){g->gain=32768;g->hold=GATE_HOLD;from=32768;}   /* open instantly */
    else if(g->env<GATE_CLOSE) {
        if(g->hold)g->hold--;
        else g->gain=g->gain>GATE_STEP?g->gain-GATE_STEP:0;
    }
    if(!g->gain&&!from)return;              /* closed: clean signal, no processing */
    if(g->gain==32768&&from==32768){mc_drive_block(x,params,&states[t]);return;}
    int32_t dry[32];
    for(unsigned i=0;i<32;i++)dry[i]=x[i];
    mc_drive_block(x,params,&states[t]);
    /* Release: crossfade wet -> dry across the block, at 16-bit precision. */
    for(unsigned i=0;i<32;i++) {
        int32_t w=from+(g->gain-from)*(int32_t)i/32;
        x[i]=(((dry[i]>>16)*(32768-w))+((x[i]>>16)*w))<<1;
    }
}
/* The settings the UI edits live in the sound owner's record (owner+16), which
 * is not guaranteed to be the record the audio engine's kit pointer reaches.
 * The UI clock therefore mirrors each track's 8 bytes from the owner record
 * into ui_params; the audio reads that mirror, except while a sound lock puts
 * another sound on the track (then that sound's own bytes apply). */
static uint8_t ui_params[6][MC_DRIVE_PARAMS];
static uint8_t ui_valid;                 /* bit t: ui_params[t] mirrored */
static int8_t audio_position[6];         /* filter position the audio last used */
void mc_drive_sync(void) {
    uint8_t *root=((void *(*)(void))0x400cf866u)();
    uint8_t valid=0;
    for(unsigned t=0;root&&t<6;t++) {
        const uint8_t *sound=mc_fw_owner_data(root+212+68*t);
        if(!sound)continue;
        uint8_t copy[MC_DRIVE_PARAMS];
        for(unsigned i=0;i<MC_DRIVE_PARAMS;i++)copy[i]=sound[DRIVE_AT+i];
        uint16_t sr=mc_irq_save();
        for(unsigned i=0;i<MC_DRIVE_PARAMS;i++)ui_params[t][i]=copy[i];
        mc_irq_restore(sr);
        valid|=(uint8_t)(1u<<t);
    }
    ui_valid=valid;
}
void mc_drive_audio(int32_t *blocks) {
    uint8_t *const *current=(uint8_t *const *)0x800017ecu;   /* per-track sound in use */
    uint8_t *kit=*(uint8_t *const *)0x800017e4u;             /* current kit */
    uint8_t params[6][MC_DRIVE_PARAMS];
    unsigned cost[6],total=0;
    for(unsigned t=0;t<6;t++) {
        const uint8_t *sound=current[t],*base=kit?kit+28+100*t:0;
        if(!sound)sound=base;
        const uint8_t *src=sound?sound+DRIVE_AT:0;
        if(sound==base&&(ui_valid&(1u<<t)))src=ui_params[t];
        for(unsigned i=0;i<MC_DRIVE_PARAMS;i++)params[t][i]=src?src[i]:0;
        cost[t]=stage_cost(params[t]);total+=cost[t]+(params[t][FILTER_BYTE]?FILTER_COST:0u);
    }
    while(total>DRIVE_BUDGET) {
        unsigned worst=0;
        if(!cost[0]&&!cost[1]&&!cost[2]&&!cost[3]&&!cost[4]&&!cost[5])break;   /* filters alone */
        for(unsigned t=1;t<6;t++)if(cost[t]>cost[worst])worst=t;
        uint8_t *d=params[worst];
        if(d[MC_DRIVE_FOLD])d[MC_DRIVE_FOLD]=0;
        else if(d[MC_DRIVE_BIAS])d[MC_DRIVE_BIAS]=0;
        else if(d[MC_DRIVE_LOW])d[MC_DRIVE_LOW]=0;
        else if(d[MC_DRIVE_TONE]||d[MC_DRIVE_DRIVE]){d[MC_DRIVE_TONE]=0;d[MC_DRIVE_DRIVE]=0;}
        else d[MC_DRIVE_AMOUNT]=0;
        unsigned now=stage_cost(d);total-=cost[worst]-now;cost[worst]=now;
    }
    for(unsigned t=0;t<6;t++) {
        int8_t position=(int8_t)params[t][FILTER_BYTE];uint8_t resonance=params[t][RESONANCE_BYTE];
        audio_position[t]=position;
        params[t][FILTER_BYTE]=0;params[t][RESONANCE_BYTE]=0;   /* not distortion settings */
        gated_block(blocks+32*t,params[t],t);
        /* DJ filter after the gated distortion, before pan/mixer/sends. */
        mc_djfilter_block(blocks+32*t,position,resonance,&filters[t]);
    }
}

/* ---- Persistence ----------------------------------------------------------- */
extern int mc_sound_save2_original(uint8_t *,const uint8_t *);
extern int mc_sound_save1_original(uint8_t *,const uint8_t *);
extern int mc_sound_load_original(uint8_t *,const uint8_t *);
/* Serialized record: the 8 bytes at +76..+83 and a signature at +88..+91
 * ('D','1', sum, sum^0xA5). The stock saver zeroes +28..+91, so no record from
 * another firmware carries the signature. In particular Model-TG (RC1..RC9)
 * kept its own per-sound data at +76..+87: without the signature those bytes
 * would load as random distortion settings. Unsigned records load with the
 * stage off (the stock loader has already zeroed runtime +20..+85). */
#define SIGNATURE_AT 88
static uint8_t record_sum(const uint8_t *b) {
    unsigned sum=0;for(unsigned i=0;i<MC_DRIVE_PARAMS;i++)sum+=b[DRIVE_AT+i];return (uint8_t)sum;
}
static void store(uint8_t *storage,const uint8_t *runtime) {
    for(unsigned i=0;i<MC_DRIVE_PARAMS;i++)storage[DRIVE_AT+i]=runtime[DRIVE_AT+i];
    uint8_t sum=record_sum(storage);
    storage[SIGNATURE_AT]='D';storage[SIGNATURE_AT+1]='1';storage[SIGNATURE_AT+2]=sum;storage[SIGNATURE_AT+3]=(uint8_t)(sum^0xa5);
}
static void restore(uint8_t *runtime,const uint8_t *storage) {
    uint8_t sum=record_sum(storage);
    if(storage[SIGNATURE_AT]!='D'||storage[SIGNATURE_AT+1]!='1'||storage[SIGNATURE_AT+2]!=sum||storage[SIGNATURE_AT+3]!=(uint8_t)(sum^0xa5))return;
    for(unsigned i=0;i<MC_DRIVE_PARAMS;i++)runtime[DRIVE_AT+i]=storage[DRIVE_AT+i];
}
int mc_sound_save2_hook(uint8_t *storage,const uint8_t *runtime) {
    int result=mc_sound_save2_original(storage,runtime);
    if(storage&&runtime)store(storage,runtime);
    return result;
}
int mc_sound_save1_hook(uint8_t *storage,const uint8_t *runtime) {
    int result=mc_sound_save1_original(storage,runtime);
    if(storage&&runtime)store(storage,runtime);
    return result;
}
int mc_sound_load_hook(uint8_t *runtime,const uint8_t *storage) {
    int result=mc_sound_load_original(runtime,storage);
    if((result&255)&&runtime&&storage)restore(runtime,storage);
    return result;
}

/* ---- Overlay: a stock Popup with our render, closing ~1 s after the last turn. */
static void *overlay;
static int overlay_value;
static char overlay_kind;   /* 'S' distortion amount, 'F' filter position */
static unsigned overlay_track;
static uint8_t overlay_all;      /* value applies to all six tracks (Control All) */
static uint32_t vtable[24];      /* copy of the Popup vtable group 0x40111bc8 */
static int vtable_ready;
static void repaint(void) {
    void *controller=((void *(*)(void))0x400d0974u)();
    void *view=((void *(*)(void *))0x400060d8u)(controller);
    if(view)((void (*)(void *))0x40076c04u)(view);
}
static int drive_dtor0(void *self,int flags) {
    if(self==overlay)overlay=0;
    return ((int (*)(void *,int))0x400d8680u)(self,flags);
}
static int drive_dtor1(void *self,int flags) {
    if(self==overlay)overlay=0;
    return ((int (*)(void *,int))0x400d8738u)(self,flags);
}
static void rect(void *ctx,int x0,int y0,int x1,int y1,int mode) {
    ((void (*)(void *,int,int,int,int,int))0x40070deau)(ctx,x0,y0,x1,y1,mode);
}
/* Screen is 128x64 with y counted from the bottom. */
static void big_number(void *ctx,unsigned value,int x0) {
    void *font=((void *(*)(const void *))0x400720f8u)((const void *)0x40ea14dcu);
    int width=((int (*)(const void *,int))0x40072272u)((const void *)0x40ea14dcu,0);
    unsigned digits[3]={value/100,value/10%10,value%10},first=value>=100?0:value>=10?1:2;
    for(unsigned i=first,x=(unsigned)x0;i<3;i++,x+=(unsigned)width+2)
        ((void (*)(void *,void *,int,int,int,int,const char *,const char *,unsigned))0x40071c10u)
            (ctx,font,(int)x,27,0,0,(const char *)0x4012772eu,(const char *)0x4012a9eau,digits[i]);
}
static void small_text(void *ctx,int x,int y,const char *text) {
    ((void (*)(void *,const void *,int,int,int,const char *,const char *))0x40071a04u)
        (ctx,(const void *)0x40ea14ccu,x,y,0x10,(const char *)0x40124b58u,text);
}
/* Screen is 128x64 with y counted from the bottom. */
/* 5x7 capitals for the Control All banner, rows top to bottom. */
static const uint8_t glyph_c[7]={0x0f,0x10,0x10,0x10,0x10,0x10,0x0f},glyph_t[7]={0x1f,4,4,4,4,4,4},
    glyph_r[7]={0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},glyph_l[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
    glyph_a[7]={0x0e,0x11,0x11,0x1f,0x11,0x11,0x11};
/* One glyph at scale px, bottom-left at (x, y); runs of lit pixels per row. */
static void big_glyph(void *ctx,const uint8_t rows[7],int x,int y,int px) {
    for(int r=0;r<7;r++) {
        int yb=y+(6-r)*px;
        for(int c=0;c<5;) {
            if(!(rows[r]&(0x10>>c))){c++;continue;}
            int e=c;while(e<5&&(rows[r]&(0x10>>e)))e++;
            rect(ctx,x+c*px,yb,x+e*px-1,yb+px-1,1);
            c=e;
        }
    }
}
static void ctrl_all_banner(void *ctx) {
    /* "CTRL" over "ALL", 3 px per dot: 15 x 21 glyphs. */
    const uint8_t *top[4]={glyph_c,glyph_t,glyph_r,glyph_l},*bottom[3]={glyph_a,glyph_l,glyph_l};
    for(int i=0;i<4;i++)big_glyph(ctx,top[i],29+i*18,36,3);
    for(int i=0;i<3;i++)big_glyph(ctx,bottom[i],38+i*18,6,3);
}
static void drive_render(void *self,void *ctx) {
    (void)self;
    rect(ctx,0,0,127,63,0);
    if(overlay_kind=='C'){ctrl_all_banner(ctx);return;}
    if(overlay_all)small_text(ctx,104,18,"ALL");
    rect(ctx,8,6,119,14,1);rect(ctx,9,7,118,13,0);          /* bar outline */
    if(overlay_kind=='F') {
        /* A large "F": stem and two arms. */
        rect(ctx,8,24,11,53,1);rect(ctx,8,50,25,53,1);rect(ctx,8,37,21,40,1);
        int v=overlay_value;unsigned m=(unsigned)(v<0?-v:v);
        small_text(ctx,40,44,v<0?"LP":v>0?"HP":"OFF");
        if(m)big_number(ctx,m,64);
        /* "ok" once the audio stage is filtering with this very setting. */
        small_text(ctx,104,44,audio_position[overlay_track%6]==v?"ok":"..");
        /* Bipolar bar from the centre: left low-pass, right high-pass. */
        rect(ctx,63,7,64,13,1);
        if(v<0)rect(ctx,63-(int)(m*52u/63u),8,63,12,1);
        if(v>0)rect(ctx,64,8,64+(int)(m*52u/63u),12,1);
        return;
    }
    /* A large "S" from five bars, 18 px wide, 30 px tall, top-left at (8,10). */
    rect(ctx,8,50,25,53,1);  rect(ctx,8,37,11,53,1);  rect(ctx,8,37,25,40,1);
    rect(ctx,22,24,25,40,1); rect(ctx,8,24,25,27,1);
    unsigned amount=(unsigned)overlay_value;
    big_number(ctx,amount,40);
    if(amount)rect(ctx,10,8,10+(int)(amount*107u/127u),12,1);
}
static void overlay_show_for(char kind,int value,int ticks);
static void overlay_show(char kind,int value){overlay_show_for(kind,value,60);}
static void overlay_show_for(char kind,int value,int ticks) {
    overlay_kind=kind;overlay_value=value;
    if(!overlay) {
        mc_popup_last=0;
        mc_show_popup("");
        overlay=mc_popup_last;
        if(!overlay)return;
        if(!vtable_ready) {
            const uint32_t *group=(const uint32_t *)0x40111bc8u;
            for(unsigned i=0;i<24;i++)vtable[i]=group[i];
            /* Primary vptr is group+8: slot +0/+4 dtors, +16 render. */
            vtable[2]=(uint32_t)(uintptr_t)drive_dtor0;
            vtable[3]=(uint32_t)(uintptr_t)drive_dtor1;
            vtable[6]=(uint32_t)(uintptr_t)drive_render;
            vtable_ready=1;
        }
        *(uint32_t *)overlay=(uint32_t)(uintptr_t)&vtable[2];
    }
    *(int32_t *)((uint8_t *)overlay+0x60)=ticks;   /* 60: about one second */
    repaint();
}

/* ---- FUNC + SWEEP / FUNC + PITCH ------------------------------------------ */
extern volatile uint8_t mc_ca_on;
static uint8_t pending_tracks;   /* bit t: owner t changed, notify once turning stops */
static uint32_t pending_since;
static int adjust(unsigned t,unsigned at,int delta,int low,int high,int is_signed) {
    uint8_t *root=((void *(*)(void))0x400cf866u)();
    uint8_t *sound=root?mc_fw_owner_data(root+212+68*t):0;
    if(!sound)return -1000;
    int value=is_signed?(int)(int8_t)sound[at]:(int)sound[at];
    value+=delta;
    if(value<low)value=low;
    if(value>high)value=high;
    uint16_t sr=mc_irq_save();
    sound[at]=(uint8_t)value;
    ui_params[t][at-DRIVE_AT]=(uint8_t)value;
    mc_irq_restore(sr);
    pending_tracks|=(uint8_t)(1u<<t);
    return value;
}
int mc_drive_encoder(const uint8_t *event,unsigned track) {
    unsigned encoder=((unsigned)event[12]<<24|(unsigned)event[13]<<16|(unsigned)event[14]<<8|event[15]);
    if((encoder!=SWEEP_ENCODER&&encoder!=PITCH_ENCODER)||track>=6)return 0;
    if(!((int (*)(int))0x4007faf4u)(1))return 0;              /* FUNC held */
    if(!((void *(*)(void))0x400cf866u)())return 1;
    int delta=((int (*)(const void *,int,int))0x4006f73au)(event,1,4);
    int sweep=encoder==SWEEP_ENCODER;
    unsigned at=DRIVE_AT+(sweep?MC_DRIVE_AMOUNT:FILTER_BYTE);
    int low=sweep?0:-63,high=sweep?127:63,shown=-1000;
    /* Control All: every track moves by the same amount from its own value. */
    for(unsigned t=0;t<6;t++) {
        if(t!=track&&!mc_ca_on)continue;
        int v=adjust(t,at,delta,low,high,!sweep);
        if(t==track)shown=v;
    }
    if(shown==-1000)return 1;
    pending_since=mc_now();
    overlay_track=track;overlay_all=mc_ca_on;
    overlay_show(sweep?'S':'F',shown);
    return 1;
}
/* UI clock: once turning stops, tell the sound owners so the change is stored. */
void mc_drive_tick(uint32_t now) {
    if(pending_tracks&&(uint32_t)(now-pending_since)>=NOTIFY_AFTER_MS) {
        uint8_t *root=((void *(*)(void))0x400cf866u)();
        uint8_t tracks=pending_tracks;pending_tracks=0;
        for(unsigned t=0;root&&t<6;t++)if(tracks&(1u<<t))mc_fw_owner_notify(root+212+68*t,0);
    }
}

/* Overlay access for the Control All banner (firmware/ctrlall.c). */
void mc_drive_overlay(char kind,int value,int ticks){if(kind=='C')overlay_all=0;overlay_show_for(kind,value,ticks);}
char mc_drive_overlay_kind(void){return overlay?overlay_kind:0;}
void mc_drive_overlay_close(void){if(overlay)*(int32_t *)((uint8_t *)overlay+0x60)=1;}
