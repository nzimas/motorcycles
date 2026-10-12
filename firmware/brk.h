#ifndef MC_BRK_H
#define MC_BRK_H
#include <stdint.h>
/* Live break (firmware/brk.c): while PAGE is held during playback, the
 * pattern is played through a variation drawn fresh at each press. Nothing is
 * written to the pattern: the sequencer's step lookup, the trig message, the
 * step locks and the per-track audio stage are reshaped on the fly. */
enum { BRK_REPEAT, BRK_HALF, BRK_DOUBLE, BRK_REVERSE, BRK_CHOP, BRK_ROLL, BRK_RECIPES };
enum { BRK_PAN=1, BRK_TIMBRE=2, BRK_FILTER=4, BRK_DRIVE=8, BRK_OCTAVE=16, BRK_RATCHET=32 };
typedef struct {
    uint8_t active, recipe, layers, window;     /* window: repeat/reverse/chop size in steps */
    uint8_t anchor[6];                          /* per-track step at the press */
    uint8_t chop[16];                           /* chop: source slice for each slice of the bar */
    uint8_t ratchet_odds[6];                    /* % of hits that roll (non-kick mostly) */
    int8_t filter_target[6];                    /* DJ filter position reached by the sweep, 0 = untouched */
    uint8_t drive_push[6];                      /* distortion amount added */
    uint8_t pan_width, timbre_depth, octave_odds;
    uint32_t seed, blocks;                      /* audio blocks since the press (sweeps, roll build-up) */
    uint32_t hits[6];                           /* trigs played per track since the press */
} mc_break;
extern volatile mc_break mc_brk;
/* UI: start (fresh variation) / stop. steps: each track's current step at the press. */
void mc_break_begin(uint32_t seed,const uint8_t steps[6]);
void mc_break_end(void);
/* Sequencer: the step whose content plays on step `step` of a track of length `length` (1..64). */
unsigned mc_break_source(unsigned track,unsigned step,unsigned length);
/* Trig: ratchet decision for this hit; returns retrig rate code (0 = none) and
   sets *count (retrig length code). */
unsigned mc_break_ratchet(unsigned track,unsigned *length_code);
/* Locks of one trig: append/override entries {k, value 8.8} in a stock lock
 * buffer (count at +8, entries of 8 bytes at +20); sound = the track's sound
 * record (parameter words at +20+2k) or 0. Returns the new count. */
int32_t mc_break_locks(unsigned track,uint8_t *buffer,int32_t count,int32_t capacity,const uint8_t *sound,unsigned step);
/* Audio: per-track overrides of the distortion amount and filter position. */
void mc_break_audio(uint8_t params[6][8]);
#endif
