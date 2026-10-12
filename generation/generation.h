#ifndef MC_GENERATION_H
#define MC_GENERATION_H
#include <stdint.h>
#include <stddef.h>
#include "music.h"
#define MC_TRACKS 6
/* Machines on the selected build: stock KICK..TONE 0..4, LYRA 5 (replaces CHORD) and MACRO (Braids) 6.
   Model-TG and its Sampler are not part of this firmware. */
#define MC_MACRO 6
#define MC_MACHINES 7
#define MC_MACRO_BIT (1u<<MC_MACRO)
#define MC_STEPS 64
/* Semantic staging ABI, deliberately NOT a firmware memory layout. */
enum mc_generator { MC_PATCH, MC_KIT, MC_PHRASE, MC_PATTERN };
enum mc_path { MC_ABSTRACT, MC_RHYTHMIC, MC_MELODIC, MC_FREE };
enum mc_role { MC_ANCHOR, MC_IMPACT, MC_DETAIL, MC_TEXTURE, MC_PITCHED, MC_RESPONSE };
typedef struct { uint8_t machine,pitch,color,shape,sweep,contour,punch,gate,finetune,decay; } mc_sound;
#define MC_STEP_LOCKS 4
/* One sequencer step. on=1 is a note trig; trigless=1 (with on=0) is a lock-only
   trig. micro: -23..23 (1/384 note). retrig: 0 none, else 1+rate code (1/1..1/80).
   retrig_length: 1..12 steps. retrig_fade: velocity curve -128..127.
   condition: 0 none, else 1+stock condition code (0..63). Locks: sound word k
   (1..3,5..8,10..22; never machine k9 or LFO destination k4) with an integer
   value, lock_count of them. */
typedef struct { uint8_t on,note,velocity,length; int8_t micro; uint8_t retrig,retrig_length; int8_t retrig_fade; uint8_t condition,trigless,lock_count,reserved; uint8_t lock_k[MC_STEP_LOCKS],lock_v[MC_STEP_LOCKS]; } mc_event;
typedef struct { uint8_t length,role; mc_event step[MC_STEPS]; } mc_phrase;
/* Per-sound controls outside the machine engine, in sound word order:
   LFO k1..k8 (speed, multiplier, fade, destination k, waveform, start phase,
   trig mode, depth), then Volume+Dist k19, delay send k20, reverb send k21,
   pan k22. Bipolar values are centred on 64. */
/* drive[8]: the per-track stage record: distortion amount, drive, fold, bias,
   tone, low (firmware/drive.h), then DJ filter position (int8, 0 = off) and
   resonance (firmware/djfilter.h); all zero = off. Stored at sound +76..+83. */
typedef struct { uint8_t lfo_speed,lfo_mult,lfo_fade,lfo_dest,lfo_wave,lfo_phase,lfo_mode,lfo_depth,volume,delay,reverb,pan; uint8_t drive[8]; } mc_sound_ext;
typedef struct { mc_sound sound[MC_TRACKS]; mc_phrase phrase[MC_TRACKS]; uint8_t sound_mask,sequence_mask; uint16_t tempo; mc_sound_ext extra[MC_TRACKS]; uint8_t volume_mask; } mc_plan;
/* volume_mask: tracks whose sound VOLUME+DIST is set to 127 without other
   sound changes (Pattern sets all six). */
typedef struct { uint8_t generator,path,target,density,brutality,machine_mask; uint8_t source_sound_valid_mask,music; uint32_t seed; uint8_t machine_valid_mask,current_machine[MC_TRACKS],phrase_length; mc_sound current_sound[MC_TRACKS]; uint8_t default_note[MC_TRACKS],note_valid_mask,locks_elsewhere; } mc_request;
/* locks_elsewhere: locked (track, slot) pairs other tracks already use; a pattern
   saves at most MC_LOCK_PAIRS of them (stock saver, 79 records). */
#define MC_LOCK_PAIRS 79
typedef char mc_request_abi_size_check[(sizeof(mc_request)==88&&offsetof(mc_request,seed)==8&&offsetof(mc_request,phrase_length)==19&&offsetof(mc_request,current_sound)==20&&offsetof(mc_request,default_note)==80&&offsetof(mc_request,note_valid_mask)==86&&sizeof(mc_sound)==10)?1:-1];
/* Fills caller-owned scratch only. Invalid input leaves destination untouched.
   Publication must validate and commit via a separately proven OS adapter. */
int mc_generate(const mc_request *,mc_plan *);
int mc_density_active(unsigned generator);
int mc_brutality_active(unsigned generator,unsigned path);
/* Encode ONLY verified engine parameter slots in an existing 38-byte buffer.
   Preserves slots0..8; does not publish, encode a sound file, or apply gain. */
int mc_encode_engine(const mc_sound *,uint8_t *,unsigned);
/* Encode the LFO (k1..k8), Volume+Dist, sends and pan (k19..k22) and the
   distortion record (bytes +56..+63 from k0, i.e. sound +76..+83) into the
   sound words starting at k0. Validates every value against its descriptor
   range; leaves the buffer untouched on failure. */
int mc_encode_extras(const mc_sound_ext *,uint8_t *,unsigned);
#endif
