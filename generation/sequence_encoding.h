#ifndef MC_SEQUENCE_ENCODING_H
#define MC_SEQUENCE_ENCODING_H
#include "generation.h"
#define MC_TRACK_BYTES 722
#define MC_LOCK_BYTES 4385
/* Number of slot-used flags set on a staged track (locks+4352..4384). */
unsigned mc_lock_pairs(const uint8_t *locks);
/* Canonical/UI track staging; does not locate or publish a pattern.
   duration_codes[d] must be supplied from verified stock timing for duration
   d in sixteenth-note steps;0 unused,1..12 required if generated events use it.
   Related track chance719 is reset to100 to remove stale zero-probability.
   update_length changes the BE word713 only for nonempty phrase lengths.
   Invalid inputs preserve both output buffers. old_track may equal new_track;
   all other input/output overlaps and pointer-range wrapping are rejected. */
int mc_stage_sequence(const mc_phrase *phrase,const uint8_t *old_track,
 uint8_t *new_track,uint8_t *new_locks,const uint8_t duration_codes[13],
 unsigned update_length);
#endif
