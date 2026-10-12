#ifndef MC_NATIVE_DURATION_H
#define MC_NATIVE_DURATION_H
#include <stdint.h>
/* Convert integer local sequencer steps to stock finite note-length codes.
 * table is the verified 128-entry ROM table at OS1.13 0x4010d814.
 * Triplet speeds use nearest representable length, ties prefer shorter.
 * On failure output remains unchanged. */
int mc_duration_codes(unsigned speed,const uint32_t table[128],uint8_t out[13]);
#endif
